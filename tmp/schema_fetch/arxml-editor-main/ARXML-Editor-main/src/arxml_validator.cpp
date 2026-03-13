// arxml_validator.cpp
//
// Validator implementation using xmllint command-line tool

#include "arxml_validator.hpp"
#include "arxml_model.hpp"

#include <QTemporaryFile>
#include <QProcess>
#include <QDir>
#include <QFileInfo>

QString ArxmlValidator::validate(const ArxmlModel &model, const QString &schemaFile) const
{
    // Write the current document to a temporary file
    QTemporaryFile tmpFile(QDir::tempPath() + QLatin1String("/arxml_validateXXXXXX.arxml"));
    if (!tmpFile.open()) {
        return QStringLiteral("Failed to create temporary file for validation.");
    }
    tmpFile.close();

    // Save model to temp file
    if (!model.saveToFile(tmpFile.fileName())) {
        return QStringLiteral("Failed to write document to temporary file.");
    }

    // Prepare arguments for xmllint
    QStringList args;
    args << "--noout" << "--schema" << schemaFile << tmpFile.fileName();

    QProcess process;
    process.start("xmllint", args);
    if (!process.waitForFinished(10000)) {
        return QStringLiteral("xmllint did not finish within the timeout.");
    }

    const QByteArray stderrOutput = process.readAllStandardError();
    
    // A zero exit code indicates success. If non-zero, return stderr or a
    // generic error message.
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return QString();
    }
    if (!stderrOutput.isEmpty()) {
        return QString::fromUtf8(stderrOutput);
    }
    return QStringLiteral("Unknown validation error.");
}
