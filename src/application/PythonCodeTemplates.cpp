#include "application/PythonCodeTemplates.h"

#include "domain/NodeTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QPair>
#include <QRegularExpression>

namespace vws::application {

namespace NodeTypes = domain::NodeTypes;

namespace {

QString pythonStringLiteral(const QString& value)
{
    auto json = QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
    json.remove(0, 1);
    json.chop(1);
    return json;
}

QString agentInputMode(DataTransferTemplate transferTemplate)
{
    return transferTemplate == DataTransferTemplate::FileToData || transferTemplate == DataTransferTemplate::FileToFile
        ? QStringLiteral("file")
        : QStringLiteral("data");
}

QString agentOutputMode(DataTransferTemplate transferTemplate)
{
    return transferTemplate == DataTransferTemplate::DataToFile || transferTemplate == DataTransferTemplate::FileToFile
        ? QStringLiteral("file")
        : QStringLiteral("data");
}

QString ioSpecCommentForTemplate(DataTransferTemplate transferTemplate)
{
    switch (transferTemplate) {
    case DataTransferTemplate::EmptyOutput:
    case DataTransferTemplate::DataOutput:
    case DataTransferTemplate::FileOutput:
        return QStringLiteral(
            "# VWS port circles:\n"
            "# - Starter nodes have no input port.\n"
            "# - Only these # vws:output comments define output circle count; runtime data never changes it.\n"
            "# - If the comment is removed, the node uses one default output circle.\n"
            "# - Change dimension=N and provide N comma-separated labels to create multiple output circles.\n"
            "# - For multi-output, return outputs[\"output\"] as a list; slot 0 sends output[0], slot 1 sends output[1].\n"
            "# - Labels are visual only: labels=a,b means output[0] must be the value for a and output[1] for b.\n"
            "# - Even dimension=1 uses a one-item list: outputs[\"output\"] = [data_for_slot_0].\n"
            "# vws:output output dimension=1 labels=1\n");
    case DataTransferTemplate::DataToData:
    case DataTransferTemplate::DataToFile:
    case DataTransferTemplate::FileToData:
    case DataTransferTemplate::FileToFile:
        return QStringLiteral(
            "# VWS port circles:\n"
            "# - Only these # vws:input/output comments define circle count; runtime data never changes it.\n"
            "# - If the comments are removed, the node uses one default input circle and one default output circle.\n"
            "# - Change dimension=N and provide N comma-separated labels to create multiple circles.\n"
            "# - The logical port names stay \"input\" and \"output\"; circle slots are list indexes inside those ports.\n"
            "# - inputs.get(\"input\", []) is always a list. Use input_data[0].get(\"field\") for slot 0.\n"
            "# - outputs[\"output\"] is always a list. slot 0 sends output[0], slot 1 sends output[1].\n"
            "# - Labels are visual only: labels=a,b means input_data[0]/output[0] is a and input_data[1]/output[1] is b.\n"
            "# vws:input input dimension=1 labels=1\n"
            "# vws:output output dimension=1 labels=1\n");
    }
    return {};
}

QString normalizedOutputFileName(const QString& fileName)
{
    const auto trimmed = fileName.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("output.csv") : trimmed;
}

bool replacePythonAssignment(QString* code, const QString& variableName, const QString& expression, QString* errorMessage)
{
    const QRegularExpression pattern(
        QStringLiteral(R"(^(\s*)%1\s*=.*$)").arg(QRegularExpression::escape(variableName)),
        QRegularExpression::MultilineOption);
    const auto match = pattern.match(*code);
    if (!match.hasMatch()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot find Python assignment line: %1 = ...").arg(variableName);
        }
        return false;
    }

    const auto replacement = QStringLiteral("%1%2 = %3")
        .arg(match.captured(1), variableName, expression);
    code->replace(match.capturedStart(0), match.capturedLength(0), replacement);
    return true;
}

} // namespace

QString PythonCodeTemplates::templateKey(DataTransferTemplate transferTemplate)
{
    switch (transferTemplate) {
    case DataTransferTemplate::EmptyOutput:
        return QStringLiteral("starter_empty_output");
    case DataTransferTemplate::DataOutput:
        return QStringLiteral("starter_data_output");
    case DataTransferTemplate::FileOutput:
        return QStringLiteral("starter_file_output");
    case DataTransferTemplate::DataToData:
        return QStringLiteral("data_to_data");
    case DataTransferTemplate::DataToFile:
        return QStringLiteral("data_to_file");
    case DataTransferTemplate::FileToData:
        return QStringLiteral("file_to_data");
    case DataTransferTemplate::FileToFile:
        return QStringLiteral("file_to_file");
    }
    return QStringLiteral("data_to_data");
}

DataTransferTemplate PythonCodeTemplates::transferTemplateFromKey(const QString& key, DataTransferTemplate fallback)
{
    const auto normalized = key.trimmed().toLower();
    if (normalized == "starter_empty_output") {
        return DataTransferTemplate::EmptyOutput;
    }
    if (normalized == "starter_data_output") {
        return DataTransferTemplate::DataOutput;
    }
    if (normalized == "starter_file_output") {
        return DataTransferTemplate::FileOutput;
    }
    if (normalized == "data_to_data") {
        return DataTransferTemplate::DataToData;
    }
    if (normalized == "data_to_file") {
        return DataTransferTemplate::DataToFile;
    }
    if (normalized == "file_to_data") {
        return DataTransferTemplate::FileToData;
    }
    if (normalized == "file_to_file") {
        return DataTransferTemplate::FileToFile;
    }
    return fallback;
}

QString PythonCodeTemplates::codeForTemplate(DataTransferTemplate transferTemplate)
{
    switch (transferTemplate) {
    case DataTransferTemplate::EmptyOutput:
        return starterEmptyOutputCode();
    case DataTransferTemplate::DataOutput:
        return starterDataOutputCode();
    case DataTransferTemplate::FileOutput:
        return starterFileOutputCode();
    case DataTransferTemplate::DataToData:
        return functionDataToDataCode();
    case DataTransferTemplate::DataToFile:
        return functionDataToFileCode();
    case DataTransferTemplate::FileToData:
        return functionFileToDataCode();
    case DataTransferTemplate::FileToFile:
        return functionFileToFileCode();
    }
    return functionDataToDataCode();
}

bool PythonCodeTemplates::isFileOutputTemplate(DataTransferTemplate transferTemplate)
{
    return transferTemplate == DataTransferTemplate::FileOutput
        || transferTemplate == DataTransferTemplate::DataToFile
        || transferTemplate == DataTransferTemplate::FileToFile;
}

QString PythonCodeTemplates::defaultOutputFileName()
{
    return QStringLiteral("output.csv");
}

QString PythonCodeTemplates::outputFileNameFromCode(const QString& code)
{
    const QRegularExpression pattern(
        QStringLiteral(R"(^\s*output_file_path\s*=\s*["']([^"'\r\n]+)["'])"),
        QRegularExpression::MultilineOption);
    const auto match = pattern.match(code);
    if (!match.hasMatch()) {
        return defaultOutputFileName();
    }
    return normalizedOutputFileName(match.captured(1));
}

QString PythonCodeTemplates::codeWithOutputFileName(const QString& code, const QString& fileName)
{
    auto updated = code;
    tryApplyOutputFileName(code, fileName, &updated);
    return updated;
}

bool PythonCodeTemplates::tryApplyOutputFileName(const QString& code, const QString& fileName, QString* updatedCode, QString* errorMessage)
{
    if (updatedCode == nullptr) {
        return false;
    }

    auto updated = code;
    if (!replacePythonAssignment(
            &updated,
            QStringLiteral("output_file_path"),
            pythonStringLiteral(normalizedOutputFileName(fileName)),
            errorMessage)) {
        return false;
    }

    *updatedCode = updated;
    return true;
}

bool PythonCodeTemplates::tryApplyAgentSettings(
    const QString& code,
    const QString& url,
    const QString& model,
    const QString& apiKey,
    int maxRetries,
    const QString& backgroundPrompt,
    const QString& taskPrompt,
    QString* updatedCode,
    QString* errorMessage)
{
    if (updatedCode == nullptr) {
        return false;
    }

    auto updated = code;
    const QList<QPair<QString, QString>> replacements = {
        {QStringLiteral("base_url"), QStringLiteral("%1.strip()").arg(pythonStringLiteral(url))},
        {QStringLiteral("model_name"), QStringLiteral("%1.strip()").arg(pythonStringLiteral(model))},
        {QStringLiteral("api_key"), QStringLiteral("%1.strip()").arg(pythonStringLiteral(apiKey))},
        {QStringLiteral("max_retries"), QStringLiteral("max(1, int(%1))").arg(maxRetries)},
        {QStringLiteral("background_prompt"), pythonStringLiteral(backgroundPrompt)},
        {QStringLiteral("task_prompt"), pythonStringLiteral(taskPrompt)},
    };

    for (const auto& replacement : replacements) {
        if (!replacePythonAssignment(&updated, replacement.first, replacement.second, errorMessage)) {
            return false;
        }
    }

    *updatedCode = updated;
    return true;
}

QString PythonCodeTemplates::starterEmptyOutputCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::EmptyOutput) + QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Starter node with no initial business payload.\n"
        "\n"
        "    Starter nodes do not accept upstream inputs. Use this template when the\n"
        "    workflow only needs a trigger, or when downstream nodes create their own data.\n"
        "    \"\"\"\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [{}],\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::starterDataOutputCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::DataOutput) + QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Create the first small JSON-serializable business data package.\"\"\"\n"
        "    # For multiple output circles, change the # vws:output dimension above\n"
        "    # and return a list with the same number of items.\n"
        "    output_data = {\n"
        "        # Add your business fields here, for example:\n"
        "        # \"field_name\": \"value\",\n"
        "    }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [output_data],\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::starterFileOutputCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::FileOutput) + QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Create a file artifact and pass its path downstream.\n"
        "\n"
        "    The editor's Output file name field updates output_file_path below.\n"
        "    \"\"\"\n"
        "\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    output_file_path = \"output.csv\"\n"
        "    output_path = artifact_dir / output_file_path\n"
        "\n"
        "    # Write your CSV or any other file content here.\n"
        "    # CSV skeleton:\n"
        "    # import csv\n"
        "    # with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "    #     writer.writeheader()\n"
        "    #     writer.writerow({\"column_a\": \"\", \"column_b\": \"\"})\n"
        "    output_path.write_text(\"\", encoding=\"utf-8\")\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [{\n"
        "                \"file_path\": str(output_path),\n"
        "                \"format\": output_path.suffix.lstrip(\".\"),\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }]\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": output_path.suffix.lstrip(\".\"),\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                },\n"
        "            }\n"
        "        ],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionDataToDataCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::DataToData) + QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read small upstream business data and return small business data.\"\"\"\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "\n"
        "    # input_data is a list of slot values. Use input_data[0].get(\"field\") for slot 0.\n"
        "    # Write your business logic here.\n"
        "    result = {}\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [result],\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionDataToFileCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::DataToFile) + QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read small upstream business data and write a file artifact.\n"
        "\n"
        "    The editor's Output file name field updates output_file_path below.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    # input_data is a list of slot values. Use input_data[0].get(\"field\") for slot 0.\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    output_file_path = \"output.csv\"\n"
        "    output_path = artifact_dir / output_file_path\n"
        "\n"
        "    # Write your CSV or any other file content here.\n"
        "    # CSV skeleton:\n"
        "    # import csv\n"
        "    # with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "    #     writer.writeheader()\n"
        "    #     writer.writerow({\"column_a\": \"\", \"column_b\": \"\"})\n"
        "    output_path.write_text(\"\", encoding=\"utf-8\")\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [{\n"
        "                \"file_path\": str(output_path),\n"
        "                \"format\": output_path.suffix.lstrip(\".\"),\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }]\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": output_path.suffix.lstrip(\".\"),\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\"size_bytes\": output_path.stat().st_size},\n"
        "            }\n"
        "        ],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionFileToDataCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::FileToData) + QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read an upstream file path and return file metadata.\n"
        "\n"
        "    This template intentionally does not assume a file type.\n"
        "    Read the upstream file and extract the data you need.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    # input_data is a list of slot values. Use input_data[0].get(\"file_path\") for slot 0.\n"
        "    file_input = input_data[0] if input_data else {}\n"
        "    if not isinstance(file_input, dict):\n"
        "        file_input = {}\n"
        "    source_path = Path(file_input.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    source_format = file_input.get(\"format\", \"\")\n"
        "\n"
        "    result = {\n"
        "        \"file_path\": str(source_path),\n"
        "        \"format\": source_format,\n"
        "        \"size_bytes\": source_path.stat().st_size,\n"
        "        # Add derived data here.\n"
        "    }\n"
        "\n"
        "    # CSV skeleton:\n"
        "    # import csv\n"
        "    # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     reader = csv.DictReader(file)\n"
        "    #     rows = list(reader)\n"
        "    #     result[\"row_count\"] = len(rows)\n"
        "    #     result[\"columns\"] = reader.fieldnames\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [result],\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionFileToFileCode()
{
    return ioSpecCommentForTemplate(DataTransferTemplate::FileToFile) + QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read an upstream file and produce a new file artifact.\n"
        "\n"
        "    The editor's Output file name field updates output_file_path below.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    # input_data is a list of slot values. Use input_data[0].get(\"file_path\") for slot 0.\n"
        "    file_input = input_data[0] if input_data else {}\n"
        "    if not isinstance(file_input, dict):\n"
        "        file_input = {}\n"
        "    source_path = Path(file_input.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    output_file_path = \"output.csv\"\n"
        "    output_path = artifact_dir / output_file_path\n"
        "\n"
        "    # Write your transformed CSV or any other file content here.\n"
        "    # This default copies bytes from the source file.\n"
        "    # CSV skeleton:\n"
        "    # import csv\n"
        "    # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as source_file:\n"
        "    #     reader = csv.DictReader(source_file)\n"
        "    #     with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #         writer = csv.DictWriter(file, fieldnames=reader.fieldnames or [])\n"
        "    #         writer.writeheader()\n"
        "    #         for row in reader:\n"
        "    #             writer.writerow(row)\n"
        "    output_path.write_bytes(source_path.read_bytes())\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [{\n"
        "                \"file_path\": str(output_path),\n"
        "                \"source_file\": str(source_path),\n"
        "                \"format\": output_path.suffix.lstrip(\".\"),\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }]\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": output_path.suffix.lstrip(\".\"),\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\n"
        "                    \"source_file\": str(source_path),\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                },\n"
        "            }\n"
        "        ],\n"
        "    }\n");
}

QString PythonCodeTemplates::loopCode()
{
    return QStringLiteral(
        "# VWS loop node:\n"
        "# - The Loop node receives upstream business input once; inputs[\"input\"] stays unchanged for every iteration.\n"
        "# - This code runs once per iteration and generates the input for the single body node after this Loop node.\n"
        "# - Loop control data lives in context[\"loop\"], never in inputs[\"input\"] or outputs[\"output\"].\n"
        "# - context[\"loop\"][\"iter\"] starts at 1; context[\"loop\"][\"index\"] starts at 0.\n"
        "# - previous_loop_output, previous_body_output, and history are available in context[\"loop\"].\n"
        "# - To add more output circles, change the output dimension and return the same number of items in outputs[\"output\"].\n"
        "# - Labels are visual only: labels=a,b means output[0] must be the value for a and output[1] for b.\n"
        "# vws:input input dimension=1 labels=1\n"
        "# vws:output output dimension=1 labels=1\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    loop = context.get(\"loop\", {})\n"
        "    iter_index = int(loop.get(\"iter\", 1))\n"
        "    zero_based_index = int(loop.get(\"index\", 0))\n"
        "    iteration_count = int(loop.get(\"iteration_count\", 1))\n"
        "    previous_loop_output = loop.get(\"previous_loop_output\")\n"
        "    previous_body_output = loop.get(\"previous_body_output\")\n"
        "    history = loop.get(\"history\", [])\n"
        "\n"
        "    # input_data is a list of slot values and stays unchanged for every iteration.\n"
        "    # Use input_data[0].get(\"field\") for slot 0, input_data[1] for slot 1, and so on.\n"
        "    # Write per-iteration business logic here. The result below becomes the body node input for this iteration.\n"
        "    result = {}\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [result],\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::defaultFunctionCode()
{
    return functionDataToDataCode();
}

QString PythonCodeTemplates::defaultStarterCode()
{
    return starterDataOutputCode();
}

QString PythonCodeTemplates::defaultAgentUrl()
{
    return QString();
}

QString PythonCodeTemplates::defaultAgentModel()
{
    return QString();
}

int PythonCodeTemplates::defaultAgentMaxRetries()
{
    return 3;
}

QString PythonCodeTemplates::agentUrlPlaceholder()
{
    return QStringLiteral("Example: https://api.openai.com/v1");
}

QString PythonCodeTemplates::agentModelPlaceholder()
{
    return QStringLiteral("Example: gpt-4o-mini");
}

QString PythonCodeTemplates::agentApiKeyPlaceholder()
{
    return QStringLiteral("Required: OpenAI-compatible API key");
}

QString PythonCodeTemplates::defaultAgentBackgroundPrompt()
{
    return QStringLiteral("You are an agent running inside Visual Workflow Studio. Read workflow inputs and return a concise, structured result.");
}

QString PythonCodeTemplates::defaultAgentTaskPrompt()
{
    return QStringLiteral("Use the upstream workflow data to complete the task.");
}

QString PythonCodeTemplates::agentCode(
    const QString& url,
    const QString& model,
    const QString& apiKey,
    int maxRetries,
    const QString& backgroundPrompt,
    const QString& taskPrompt,
    DataTransferTemplate transferTemplate)
{
    return ioSpecCommentForTemplate(transferTemplate) + QStringLiteral(
        "import json\n"
        "import time\n"
        "import urllib.error\n"
        "import urllib.request\n"
        "from pathlib import Path\n"
        "\n"
        "def _sleep_before_retry(retry_count: int) -> None:\n"
        "    \"\"\"Sleep before the next retry.\n"
        "\n"
        "    retry_count starts from 1 for the first retry.\n"
        "    \"\"\"\n"
        "    delay_seconds = min(8, 2 ** (retry_count - 1))\n"
        "    time.sleep(delay_seconds)\n"
        "\n"
        "def _post_chat_completion(endpoint: str, payload: dict, api_key: str, timeout_seconds: int) -> dict:\n"
        "    request = urllib.request.Request(\n"
        "        endpoint,\n"
        "        data=json.dumps(payload, ensure_ascii=False).encode(\"utf-8\"),\n"
        "        headers={\n"
        "            \"Content-Type\": \"application/json\",\n"
        "            \"Authorization\": f\"Bearer {api_key}\",\n"
        "        },\n"
        "        method=\"POST\",\n"
        "    )\n"
        "    with urllib.request.urlopen(request, timeout=timeout_seconds) as response:\n"
        "        return json.loads(response.read().decode(\"utf-8\"))\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Run an OpenAI-compatible chat completion as an Agent node.\"\"\"\n"
        "    input_data = inputs.get(\"input\", [])\n"
        "    # input_data is a list of slot values. Use input_data[0].get(\"field\") for slot 0.\n"
        "    input_mode = %6\n"
        "    output_mode = %7\n"
        "\n"
        "    base_url = %1.strip()\n"
        "    model_name = %2.strip()\n"
        "    api_key = %3.strip()\n"
        "    try:\n"
        "        max_retries = max(1, int(%8))\n"
        "    except Exception:\n"
        "        max_retries = 1\n"
        "    background_prompt = %4\n"
        "    task_prompt = %5\n"
        "\n"
        "    if not base_url:\n"
        "        raise RuntimeError(\"Agent url is required.\")\n"
        "    if not model_name:\n"
        "        raise RuntimeError(\"Agent model name is required.\")\n"
        "    if not api_key:\n"
        "        raise RuntimeError(\"Agent api_key is required.\")\n"
        "\n"
        "    if input_mode == \"file\":\n"
        "        file_input = input_data[0] if input_data else {}\n"
        "        if not isinstance(file_input, dict):\n"
        "            file_input = {}\n"
        "        source_path = Path(file_input.get(\"file_path\", \"\"))\n"
        "        if not source_path.exists():\n"
        "            raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "        with source_path.open(\"r\", encoding=\"utf-8\") as file:\n"
        "            file_text = file.read()\n"
        "        workflow_input = {\n"
        "            \"file_path\": str(source_path),\n"
        "            \"size_bytes\": source_path.stat().st_size,\n"
        "            \"content\": file_text,\n"
        "        }\n"
        "    else:\n"
        "        workflow_input = input_data\n"
        "\n"
        "    endpoint = base_url.rstrip(\"/\")\n"
        "    if not endpoint.endswith(\"/chat/completions\"):\n"
        "        endpoint = endpoint + \"/chat/completions\"\n"
        "\n"
        "    user_content = task_prompt\n"
        "    if workflow_input not in ({}, [], None, \"\"):\n"
        "        user_content += \"\\n\\nWorkflow input:\\n\" + json.dumps(workflow_input, ensure_ascii=False, indent=2)\n"
        "\n"
        "    payload = {\n"
        "        \"model\": model_name,\n"
        "        \"messages\": [\n"
        "            {\"role\": \"system\", \"content\": background_prompt},\n"
        "            {\"role\": \"user\", \"content\": user_content},\n"
        "        ],\n"
        "    }\n"
        "\n"
        "    last_error = None\n"
        "    response_data = None\n"
        "    retry_count = 0\n"
        "\n"
        "    for attempt_index in range(max_retries + 1):\n"
        "        try:\n"
        "            response_data = _post_chat_completion(endpoint, payload, api_key, 120)\n"
        "            break\n"
        "        except urllib.error.HTTPError as exc:\n"
        "            body = exc.read().decode(\"utf-8\", errors=\"replace\")\n"
        "            last_error = RuntimeError(f\"HTTP {exc.code}: {body}\")\n"
        "        except urllib.error.URLError as exc:\n"
        "            last_error = exc\n"
        "        except TimeoutError as exc:\n"
        "            last_error = exc\n"
        "        except OSError as exc:\n"
        "            last_error = exc\n"
        "        except Exception as exc:\n"
        "            last_error = exc\n"
        "\n"
        "        if attempt_index < max_retries:\n"
        "            retry_count += 1\n"
        "            _sleep_before_retry(retry_count)\n"
        "\n"
        "    if response_data is None:\n"
        "        raise RuntimeError(\n"
        "            f\"Agent request failed after {retry_count} retries \"\n"
        "            f\"(max_retries={max_retries}). Last error: {last_error}\"\n"
        "        )\n"
        "\n"
        "    content = response_data[\"choices\"][0][\"message\"][\"content\"]\n"
        "    if output_mode == \"file\":\n"
        "        artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "        artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "        output_file_path = \"output.csv\"\n"
        "        output_path = artifact_dir / output_file_path\n"
        "        # The editor's Output file name field updates output_file_path above.\n"
        "        output_path.write_text(content, encoding=\"utf-8\")\n"
        "        return {\n"
        "            \"outputs\": {\n"
        "                \"output\": [{\n"
        "                    \"file_path\": str(output_path),\n"
        "                    \"format\": output_path.suffix.lstrip(\".\"),\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                    \"retry_count\": retry_count,\n"
        "                    \"max_retries\": max_retries,\n"
        "                }]\n"
        "            },\n"
        "            \"artifacts\": [\n"
        "                {\"type\": output_path.suffix.lstrip(\".\"), \"path\": str(output_path), \"metadata\": {\"size_bytes\": output_path.stat().st_size, \"retry_count\": retry_count, \"max_retries\": max_retries}}\n"
        "            ],\n"
        "        }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": [{\n"
        "                \"result\": content,\n"
        "                \"raw_response\": response_data,\n"
        "                \"retry_count\": retry_count,\n"
        "                \"max_retries\": max_retries,\n"
        "            }]\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n")
        .arg(pythonStringLiteral(url),
            pythonStringLiteral(model),
            pythonStringLiteral(apiKey),
            pythonStringLiteral(backgroundPrompt),
            pythonStringLiteral(taskPrompt),
            pythonStringLiteral(agentInputMode(transferTemplate)),
            pythonStringLiteral(agentOutputMode(transferTemplate)),
            QString::number(maxRetries));
}

QString PythonCodeTemplates::defaultAgentCode()
{
    return agentCode(
        defaultAgentUrl(),
        defaultAgentModel(),
        QString(),
        defaultAgentMaxRetries(),
        defaultAgentBackgroundPrompt(),
        defaultAgentTaskPrompt(),
        DataTransferTemplate::DataToData);
}

QString PythonCodeTemplates::defaultCodeForNodeType(const QString& nodeType)
{
    const auto normalized = nodeType.trimmed().toLower();
    if (normalized == NodeTypes::Starter) {
        return starterDataOutputCode();
    }
    if (normalized == NodeTypes::Agent) {
        return defaultAgentCode();
    }
    if (normalized == NodeTypes::Loop) {
        return loopCode();
    }
    return functionDataToDataCode();
}

} // namespace vws::application
