// arxml_validator.hpp
//
// Defines ArxmlValidator, a helper class that validates ARXML
// documents against an XSD schema using the external `xmllint` tool. This
// approach avoids requiring the Qt XmlPatterns module, which may not be
// available on all platforms, while still providing robust schema
// validation. The validator writes the current document to a temporary
// file and executes xmllint via QProcess. Any error output from the
// process is returned to the caller for display.

#ifndef ARXML_VALIDATOR_HPP
#define ARXML_VALIDATOR_HPP

#include <QString>

class ArxmlModel;

class ArxmlValidator
{
public:
    // Validate the given model against the specified schema file. Returns an
    // empty string on success, or the error output from xmllint on failure.
    QString validate(const ArxmlModel &model, const QString &schemaFile) const;
};

#endif // ARXML_VALIDATOR_HPP