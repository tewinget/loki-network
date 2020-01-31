#include <crypto/crypto_libsodium.hpp>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_scalarmult_ed25519.h>
#include <sodium/crypto_scalarmult_ristretto255.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <sodium/crypto_core_ed25519.h>
#include <sodium/crypto_core_ristretto255.h>
#include <sodium/randombytes.h>
#include <sodium/utils.h>
#include <util/mem.hpp>
#include <util/endian.hpp>
#include <cassert>
#include <cstring>

extern "C"
{
  extern int
  sodium_init(void);
}

namespace llarp
{
  namespace sodium
  {
    static bool
    dh(llarp::SharedSecret &out, const PubKey &client_pk,
       const PubKey &server_pk, const uint8_t *themPub, const SecretKey &usSec)
    {
      llarp::SharedSecret shared;
      crypto_generichash_state h;

      if(crypto_scalarmult_curve25519(shared.data(), usSec.data(), themPub))
      {
        return false;
      }
      crypto_generichash_blake2b_init(&h, nullptr, 0U, shared.size());
      crypto_generichash_blake2b_update(&h, client_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, server_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, shared.data(), 32);
      crypto_generichash_blake2b_final(&h, out.data(), shared.size());
      return true;
    }

    static bool
    dh_client_priv(llarp::SharedSecret &shared, const PubKey &pk,
                   const SecretKey &sk, const TunnelNonce &n)
    {
      llarp::SharedSecret dh_result;

      if(dh(dh_result, sk.toPublic(), pk, pk.data(), sk))
      {
        return crypto_generichash_blake2b(shared.data(), 32, n.data(), 32,
                                          dh_result.data(), 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_client - dh failed");
      return false;
    }

    static bool
    dh_server_priv(llarp::SharedSecret &shared, const PubKey &pk,
                   const SecretKey &sk, const TunnelNonce &n)
    {
      llarp::SharedSecret dh_result;
      if(dh(dh_result, pk, sk.toPublic(), pk.data(), sk))
      {
        return crypto_generichash_blake2b(shared.data(), 32, n.data(), 32,
                                          dh_result.data(), 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_server - dh failed");
      return false;
    }

    CryptoLibSodium::CryptoLibSodium()
    {
      if(sodium_init() == -1)
      {
        throw std::runtime_error("sodium_init() returned -1");
      }
      char *avx2 = std::getenv("AVX2_FORCE_DISABLE");
      if(avx2 && std::string(avx2) == "1")
      {
        ntru_init(1);
      }
      else
      {
        ntru_init(0);
      }
      int seed = 0;
      randombytes(reinterpret_cast< unsigned char * >(&seed), sizeof(seed));
      srand(seed);
    }

    bool
    CryptoLibSodium::xchacha20(const llarp_buffer_t &buff,
                               const SharedSecret &k, const TunnelNonce &n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz,
                                         n.data(), k.data())
          == 0;
    }

    bool
    CryptoLibSodium::xchacha20_alt(const llarp_buffer_t &out,
                                   const llarp_buffer_t &in,
                                   const SharedSecret &k, const byte_t *n)
    {
      if(in.sz > out.sz)
        return false;
      return crypto_stream_xchacha20_xor(out.base, in.base, in.sz, n, k.data())
          == 0;
    }

    bool
    CryptoLibSodium::dh_client(llarp::SharedSecret &shared, const PubKey &pk,
                               const SecretKey &sk, const TunnelNonce &n)
    {
      return dh_client_priv(shared, pk, sk, n);
    }
    /// path dh relay side
    bool
    CryptoLibSodium::dh_server(llarp::SharedSecret &shared, const PubKey &pk,
                               const SecretKey &sk, const TunnelNonce &n)
    {
      return dh_server_priv(shared, pk, sk, n);
    }
    /// transport dh client side
    bool
    CryptoLibSodium::transport_dh_client(llarp::SharedSecret &shared,
                                         const PubKey &pk, const SecretKey &sk,
                                         const TunnelNonce &n)
    {
      return dh_client_priv(shared, pk, sk, n);
    }
    /// transport dh server side
    bool
    CryptoLibSodium::transport_dh_server(llarp::SharedSecret &shared,
                                         const PubKey &pk, const SecretKey &sk,
                                         const TunnelNonce &n)
    {
      return dh_server_priv(shared, pk, sk, n);
    }

    bool
    CryptoLibSodium::shorthash(ShortHash &result, const llarp_buffer_t &buff)
    {
      return crypto_generichash_blake2b(result.data(), ShortHash::SIZE,
                                        buff.base, buff.sz, nullptr, 0)
          != -1;
    }

    bool
    CryptoLibSodium::hmac(byte_t *result, const llarp_buffer_t &buff,
                          const SharedSecret &secret)
    {
      return crypto_generichash_blake2b(result, HMACSIZE, buff.base, buff.sz,
                                        secret.data(), HMACSECSIZE)
          != -1;
    }

    static bool
    hash(uint8_t *result, const llarp_buffer_t &buff)
    {
      return crypto_generichash_blake2b(result, HASHSIZE, buff.base, buff.sz,
                                        nullptr, 0)
          != -1;
    }

    bool
    CryptoLibSodium::sign(Signature &sig, const SecretKey &secret,
                          const llarp_buffer_t &buf)
    {
      return crypto_sign_detached(sig.data(), nullptr, buf.base, buf.sz,
                                  secret.data())
          != -1;
    }

    bool
    CryptoLibSodium::sign(Signature &sig, const PrivateKey &secret,
                          const llarp_buffer_t &buf)
    {
      PubKey pubkey;

      secret.toPublic(pubkey);

      crypto_hash_sha512_state hs;
      unsigned char            nonce[64];
      unsigned char            hram[64];
      unsigned char            mulres[32];
      unsigned char            r_hash_input[32];

      randombytes_buf(r_hash_input, 32);

      // r = H(H(k) || M) where here H(k) is random bytes
      crypto_hash_sha512_init(&hs);
      crypto_hash_sha512_update(&hs, r_hash_input, 32);
      crypto_hash_sha512_update(&hs, buf.base, buf.sz);
      crypto_hash_sha512_final(&hs, nonce);
      crypto_core_ed25519_scalar_reduce(nonce, nonce);

      // copy pubkey into sig to make (for now) sig = (R || A)
      memmove(sig.data() + 32, pubkey.data(), 32);

      // R = r * B
      crypto_scalarmult_ed25519_base_noclamp(sig.data(), nonce);

      // hram = H(R || A || M)
      crypto_hash_sha512_init(&hs);
      crypto_hash_sha512_update(&hs, sig.data(), 64);
      crypto_hash_sha512_update(&hs, buf.base, buf.sz);
      crypto_hash_sha512_final(&hs, hram);

      // S = r + H(R || A || M) * s, so sig = (R || S)
      crypto_core_ed25519_scalar_reduce(hram, hram);
      crypto_core_ed25519_scalar_mul(mulres, hram, secret.data());
      crypto_core_ed25519_scalar_add(sig.data() + 32, mulres, nonce);

      sodium_memzero(r_hash_input, sizeof r_hash_input);
      sodium_memzero(nonce, sizeof nonce);

      return true;
    }

    bool
    CryptoLibSodium::verify(const PubKey &pub, const llarp_buffer_t &buf,
                            const Signature &sig)
    {
      return crypto_sign_verify_detached(sig.data(), buf.base, buf.sz,
                                         pub.data())
          != -1;
    }

    /// clamp a 32 byte ec point
    static void
    clamp_ed25519(byte_t *out)
    {
      out[0] &= 248;
      out[31] &= 127;
      out[31] |= 64;
    }

    template < typename K >
    static K
    clamp(const K &p)
    {
      K out = p;
      clamp_ed25519(out);
      return out;
    }

    template < typename K >
    static bool
    is_clamped(const K &key)
    {
      K other(key);
      clamp_ed25519(other.data());
      return other == key;
    }

    template < typename K >
    static bool
    make_scalar(byte_t *out, const K &k, uint64_t i)
    {
      // b = i || k
      std::array< byte_t, K::SIZE + sizeof(uint64_t) > buf;
      htole64buf(buf.data(), i);
      std::copy_n(k.begin(), K::SIZE, buf.begin() + sizeof(i));
      LongHash h;
      // n = H(b)
      if(not hash(h.data(), llarp_buffer_t(buf)))
        return false;
      // return make_point(n)
      return crypto_core_ed25519_from_uniform(out, h.data()) != -1;
    }

    static AlignedBuffer< 32 > zero;

    bool
    CryptoLibSodium::derive_subkey(PubKey &out_pubkey, const PubKey &root_pubkey,
                                   uint64_t key_n, const AlignedBuffer<32>* hash)
    {
      // scalar h = H( in_k || root_pubkey )
      AlignedBuffer< 32 > h;
      if (hash)
        h = *hash;
      else if(not make_scalar(h.data(), root_pubkey, key_n))
      {
        LogError("cannot make scalar");
        return false;
      }

      return 0 == crypto_scalarmult_ed25519(out_pubkey.data(), h.data(), root_pubkey.data());
    }

    bool
    CryptoLibSodium::derive_subkey_private(PrivateKey &out_key,
                                          const SecretKey &root_key,
                                          uint64_t key_n,
                                          const AlignedBuffer<32>* hash
                                          )
    {

      // Derives a private subkey from a root key.
      //
      // The basic idea is:
      //
      // h - hash dependent on the `key_n` value.
      // a - private key
      // A = aB - public key
      // a' = ah - derived private key
      // A' = a'B = (ah)B - derived public key
      //
      // libsodium throws some wrenches in the mechanics which are a nuisance, the biggest of which
      // is that sodium's secret key is *not* `a`; rather it is the seed.  If you want to get the
      // private key (i.e. "a"), you need to SHA-512 hash it and then clamp that.
      //
      // This also makes signature verification harder: we can't just use sodium's verify function
      // because it wants to be given the seed rather than the private key, and moreover we can't
      // actually *get* the seed to make libsodium happy because we only have `ah` above.
      //
      const auto root_pubkey = root_key.toPublic();

      // scalar h = H( in_k || root_pubkey )
      AlignedBuffer< 32 > h;
      if (hash)
        h = *hash;
      else if(not make_scalar(h.data(), root_pubkey, key_n))
      {
        LogError("cannot make scalar");
        return false;
      }

      h[0] &= 248;
      h[31] &= 63;
      h[31] |= 64;

      PrivateKey a;
      if (!root_key.toPrivate(a))
        return false;

      // a' = ha
      crypto_core_ed25519_scalar_mul(out_key.data(), h.data(), a.data());

      return true;
    }

    bool
    CryptoLibSodium::seed_to_secretkey(llarp::SecretKey &secret,
                                       const llarp::IdentitySecret &seed)
    {
      return crypto_sign_ed25519_seed_keypair(secret.data() + 32, secret.data(),
                                              seed.data())
          != -1;
    }
    void
    CryptoLibSodium::randomize(const llarp_buffer_t &buff)
    {
      randombytes((unsigned char *)buff.base, buff.sz);
    }

    void
    CryptoLibSodium::randbytes(byte_t *ptr, size_t sz)
    {
      randombytes((unsigned char *)ptr, sz);
    }

    void
    CryptoLibSodium::identity_keygen(llarp::SecretKey &keys)
    {
      PubKey pk;
      int result = crypto_sign_keypair(pk.data(), keys.data());
      assert(result != -1);
      const PubKey sk_pk = keys.toPublic();
      assert(pk == sk_pk);
      (void)result;
      (void)sk_pk;

      // encryption_keygen(keys);
    }

    bool
    CryptoLibSodium::check_identity_privkey(const llarp::SecretKey &keys)
    {
      AlignedBuffer< crypto_sign_SEEDBYTES > seed;
      llarp::PubKey pk;
      llarp::SecretKey sk;
      if(crypto_sign_ed25519_sk_to_seed(seed.data(), keys.data()) == -1)
        return false;
      if(crypto_sign_seed_keypair(pk.data(), sk.data(), seed.data()) == -1)
        return false;
      return keys.toPublic() == pk && sk == keys;
    }

    void
    CryptoLibSodium::encryption_keygen(llarp::SecretKey &keys)
    {
      auto d = keys.data();
      randbytes(d, 32);
      crypto_scalarmult_curve25519_base(d + 32, d);
    }

    bool
    CryptoLibSodium::pqe_encrypt(PQCipherBlock &ciphertext,
                                 SharedSecret &sharedkey,
                                 const PQPubKey &pubkey)
    {
      return crypto_kem_enc(ciphertext.data(), sharedkey.data(), pubkey.data())
          != -1;
    }
    bool
    CryptoLibSodium::pqe_decrypt(const PQCipherBlock &ciphertext,
                                 SharedSecret &sharedkey,
                                 const byte_t *secretkey)
    {
      return crypto_kem_dec(sharedkey.data(), ciphertext.data(), secretkey)
          != -1;
    }

    void
    CryptoLibSodium::pqe_keygen(PQKeyPair &keypair)
    {
      auto d = keypair.data();
      crypto_kem_keypair(d + PQ_SECRETKEYSIZE, d);
    }
  }  // namespace sodium

  const byte_t *
  seckey_topublic(const SecretKey &sec)
  {
    return sec.data() + 32;
  }

  const byte_t *
  pq_keypair_to_public(const PQKeyPair &k)
  {
    return k.data() + PQ_SECRETKEYSIZE;
  }

  const byte_t *
  pq_keypair_to_secret(const PQKeyPair &k)
  {
    return k.data();
  }

  uint64_t
  randint()
  {
    uint64_t i;
    randombytes((byte_t *)&i, sizeof(i));
    return i;
  }

}  // namespace llarp
