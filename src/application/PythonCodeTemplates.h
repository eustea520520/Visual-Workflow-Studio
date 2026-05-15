#pragma once

#include <QString>

namespace vws::application {

enum class DataTransferTemplate {
    EmptyOutput,
    DataOutput,
    FileOutput,
    DataToData,
    DataToFile,
    FileToData,
    FileToFile,
};

// Stores user-visible Python templates only. Runtime execution still belongs to PythonNodeWorker.
class PythonCodeTemplates final {
public:
    static QString templateKey(DataTransferTemplate transferTemplate);
    static DataTransferTemplate transferTemplateFromKey(const QString& key, DataTransferTemplate fallback);
    static QString codeForTemplate(DataTransferTemplate transferTemplate);

    static QString starterEmptyOutputCode();
    static QString starterDataOutputCode();
    static QString starterFileOutputCode();
    static QString functionDataToDataCode();
    static QString functionDataToFileCode();
    static QString functionFileToDataCode();
    static QString functionFileToFileCode();

    static QString defaultFunctionCode();
    static QString defaultStarterCode();

    static QString defaultAgentUrl();
    static QString defaultAgentModel();
    static QString agentUrlPlaceholder();
    static QString agentModelPlaceholder();
    static QString agentApiKeyPlaceholder();
    static int defaultAgentMaxRetries();
    static QString defaultAgentBackgroundPrompt();
    static QString defaultAgentTaskPrompt();
    static QString agentCode(
        const QString& url,
        const QString& model,
        const QString& apiKey,
        int maxRetries,
        const QString& backgroundPrompt,
        const QString& taskPrompt,
        DataTransferTemplate transferTemplate = DataTransferTemplate::DataToData);
    static QString defaultAgentCode();
    static QString defaultCodeForNodeType(const QString& nodeType);
};

} // namespace vws::application
