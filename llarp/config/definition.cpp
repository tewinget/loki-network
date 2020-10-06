#include <config/definition.hpp>

#include <iterator>
#include <sstream>
#include <stdexcept>
#include <cassert>

namespace llarp
{
  template <>
  bool
  OptionDefinition<bool>::fromString(const std::string& input)
  {
    if (input == "false" || input == "off" || input == "0" || input == "no")
      return false;
    else if (input == "true" || input == "on" || input == "1" || input == "yes")
      return true;
    else
      throw std::invalid_argument(stringify(input, " is not a valid bool"));
  }

  ConfigDefinition&
  ConfigDefinition::defineOption(OptionDefinition_ptr def)
  {
    if (relay ? def->clientOnly : def->relayOnly)
      return *this;

    auto [sectionItr, newSect] = m_definitions.try_emplace(def->section);
    if (newSect)
      m_sectionOrdering.push_back(def->section);
    auto& section = sectionItr->first;

    auto [it, added] = m_definitions[section].try_emplace(std::string{def->name}, std::move(def));
    if (!added)
      throw std::invalid_argument(
          stringify("definition for [", def->section, "]:", def->name, " already exists"));

    m_definitionOrdering[section].push_back(it->first);

    if (!it->second->comments.empty())
      addOptionComments(section, it->first, std::move(it->second->comments));

    return *this;
  }

  ConfigDefinition&
  ConfigDefinition::addConfigValue(
      std::string_view section, std::string_view name, std::string_view value)
  {
    // see if we have an undeclared handler to fall back to in case section or section:name is
    // absent
    auto undItr = m_undeclaredHandlers.find(std::string(section));
    bool haveUndeclaredHandler = (undItr != m_undeclaredHandlers.end());

    // get section, falling back to undeclared handler if needed
    auto secItr = m_definitions.find(std::string(section));
    if (secItr == m_definitions.end())
    {
      // fallback to undeclared handler if available
      if (not haveUndeclaredHandler)
        throw std::invalid_argument(stringify("unrecognized section [", section, "]"));
      else
      {
        auto& handler = undItr->second;
        handler(section, name, value);
        return *this;
      }
    }

    // section was valid, get definition by name
    // fall back to undeclared handler if needed
    auto& sectionDefinitions = secItr->second;
    auto defItr = sectionDefinitions.find(std::string(name));
    if (defItr == sectionDefinitions.end())
    {
      if (not haveUndeclaredHandler)
        throw std::invalid_argument(stringify("unrecognized option [", section, "]:", name));
      else
      {
        auto& handler = undItr->second;
        handler(section, name, value);
        return *this;
      }
    }

    OptionDefinition_ptr& definition = defItr->second;
    definition->parseValue(std::string(value));

    return *this;
  }

  void
  ConfigDefinition::addUndeclaredHandler(const std::string& section, UndeclaredValueHandler handler)
  {
    auto itr = m_undeclaredHandlers.find(section);
    if (itr != m_undeclaredHandlers.end())
      throw std::logic_error(stringify("section ", section, " already has a handler"));

    m_undeclaredHandlers[section] = std::move(handler);
  }

  void
  ConfigDefinition::removeUndeclaredHandler(const std::string& section)
  {
    auto itr = m_undeclaredHandlers.find(section);
    if (itr != m_undeclaredHandlers.end())
      m_undeclaredHandlers.erase(itr);
  }

  void
  ConfigDefinition::validateRequiredFields()
  {
    visitSections([&](const std::string& section, const DefinitionMap&) {
      visitDefinitions(section, [&](const std::string&, const OptionDefinition_ptr& def) {
        if (def->required and def->getNumberFound() < 1)
        {
          throw std::invalid_argument(
              stringify("[", section, "]:", def->name, " is required but missing"));
        }

        // should be handled earlier in OptionDefinition::parseValue()
        assert(def->getNumberFound() <= 1 or def->multiValued);
      });
    });
  }

  void
  ConfigDefinition::acceptAllOptions()
  {
    visitSections([&](const std::string& section, const DefinitionMap&) {
      visitDefinitions(
          section, [&](const std::string&, const OptionDefinition_ptr& def) { def->tryAccept(); });
    });
  }

  void
  ConfigDefinition::addSectionComments(
      const std::string& section, std::vector<std::string> comments)
  {
    auto& sectionComments = m_sectionComments[section];
    for (size_t i = 0; i < comments.size(); ++i)
    {
      sectionComments.emplace_back(std::move(comments[i]));
    }
  }

  void
  ConfigDefinition::addOptionComments(
      const std::string& section, const std::string& name, std::vector<std::string> comments)
  {
    auto& defComments = m_definitionComments[section][name];
    if (defComments.empty())
      defComments = std::move(comments);
    else
      defComments.insert(
          defComments.end(),
          std::make_move_iterator(comments.begin()),
          std::make_move_iterator(comments.end()));
  }

  std::string
  ConfigDefinition::generateINIConfig(bool useValues)
  {
    std::ostringstream oss;

    int sectionsVisited = 0;

    visitSections([&](const std::string& section, const DefinitionMap&) {
      if (sectionsVisited > 0)
        oss << "\n\n";

      oss << "[" << section << "]\n";

      // TODO: this will create empty objects as a side effect of map's operator[]
      // TODO: this also won't handle sections which have no definition
      for (const std::string& comment : m_sectionComments[section])
      {
        oss << "# " << comment << "\n";
      }

      visitDefinitions(section, [&](const std::string& name, const OptionDefinition_ptr& def) {
        bool has_comment = false;
        // TODO: as above, this will create empty objects
        // TODO: as above (but more important): this won't handle definitions with no entries
        //       (i.e. those handled by UndeclaredValueHandler's)
        for (const std::string& comment : m_definitionComments[section][name])
        {
          oss << "\n# " << comment;
          has_comment = true;
        }

        if (useValues and def->getNumberFound() > 0)
        {
          oss << "\n" << name << "=" << def->valueAsString(false) << "\n";
        }
        else if (not(def->hidden and not has_comment))
        {
          oss << "\n";
          if (not def->required)
            oss << "#";
          oss << name << "=" << def->defaultValueAsString() << "\n";
        }
      });

      sectionsVisited++;
    });

    return oss.str();
  }

  const OptionDefinition_ptr&
  ConfigDefinition::lookupDefinitionOrThrow(std::string_view section, std::string_view name) const
  {
    const auto sectionItr = m_definitions.find(std::string(section));
    if (sectionItr == m_definitions.end())
      throw std::invalid_argument(stringify("No config section [", section, "]"));

    auto& sectionDefinitions = sectionItr->second;
    const auto definitionItr = sectionDefinitions.find(std::string(name));
    if (definitionItr == sectionDefinitions.end())
      throw std::invalid_argument(stringify("No config item ", name, " within section ", section));

    return definitionItr->second;
  }

  OptionDefinition_ptr&
  ConfigDefinition::lookupDefinitionOrThrow(std::string_view section, std::string_view name)
  {
    return const_cast<OptionDefinition_ptr&>(
        const_cast<const ConfigDefinition*>(this)->lookupDefinitionOrThrow(section, name));
  }

  void
  ConfigDefinition::visitSections(SectionVisitor visitor) const
  {
    for (const std::string& section : m_sectionOrdering)
    {
      const auto itr = m_definitions.find(section);
      assert(itr != m_definitions.end());
      visitor(section, itr->second);
    }
  };
  void
  ConfigDefinition::visitDefinitions(const std::string& section, DefVisitor visitor) const
  {
    const auto& defs = m_definitions.at(section);
    const auto& defOrdering = m_definitionOrdering.at(section);
    for (const std::string& name : defOrdering)
    {
      const auto itr = defs.find(name);
      assert(itr != defs.end());
      visitor(name, itr->second);
    }
  };

}  // namespace llarp
