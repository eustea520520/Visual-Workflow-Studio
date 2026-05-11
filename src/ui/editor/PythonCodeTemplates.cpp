#include "ui/editor/PythonCodeTemplates.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace vws::ui {

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

QString PythonCodeTemplates::starterEmptyOutputCode()
{
    return QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Starter node with no initial business payload.\n"
        "\n"
        "    Starter nodes do not accept upstream inputs. Use this template when the\n"
        "    workflow only needs a trigger, or when downstream nodes create their own data.\n"
        "    \"\"\"\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {},\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::starterDataOutputCode()
{
    return QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Create the first small JSON-serializable business data package.\"\"\"\n"
        "    output_data = {\n"
        "        # Example:\n"
        "        # \"a\": 1,\n"
        "        # \"b\": 2,\n"
        "    }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": output_data,\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::starterFileOutputCode()
{
    return QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Create an empty CSV file and pass its path downstream.\"\"\"\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    file_path = artifact_dir / \"output.csv\"\n"
        "    with file_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "        # Write CSV content here when this starter owns the initial file.\n"
        "        # Example:\n"
        "        # import csv\n"
        "        # writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "        # writer.writeheader()\n"
        "        # writer.writerow({\"column_a\": \"value\", \"column_b\": 1})\n"
        "        pass\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(file_path),\n"
        "                \"format\": \"csv\",\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": \"csv\",\n"
        "                \"path\": str(file_path),\n"
        "                \"metadata\": {\n"
        "                    \"encoding\": \"utf-8\",\n"
        "                },\n"
        "            }\n"
        "        ],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionDataToDataCode()
{
    return QStringLiteral(
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read small upstream business data and return small business data.\"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "\n"
        "    # Business code here.\n"
        "    # Example:\n"
        "    # result = {\"value\": input_data[...].get(\"value\")}\n"
        "    result = {}\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": result,\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionDataToFileCode()
{
    return QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read small upstream business data and write a CSV file artifact.\"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    output_path = artifact_dir / \"output.csv\"\n"
        "    with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "        # Convert input_data to CSV rows here.\n"
        "        # Example:\n"
        "        # import csv\n"
        "        # writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "        # writer.writeheader()\n"
        "        # writer.writerow({\"column_a\": input_data.get(\"a\"), \"column_b\": input_data.get(\"b\")})\n"
        "        pass\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(output_path),\n"
        "                \"format\": \"csv\",\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": \"csv\",\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\"encoding\": \"utf-8\", \"size_bytes\": output_path.stat().st_size},\n"
        "            }\n"
        "        ],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionFileToDataCode()
{
    return QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read an upstream CSV file path and return small derived business data.\"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    source_path = Path(input_data.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    # Read and summarize CSV content here.\n"
        "    # Example:\n"
        "    # import csv\n"
        "    # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     reader = csv.DictReader(file)\n"
        "    #     rows = list(reader)\n"
        "    #     result = {\"row_count\": len(rows)}\n"
        "\n"
        "    result = {\n"
        "        \"file_path\": str(source_path),\n"
        "        \"format\": input_data.get(\"format\", \"csv\"),\n"
        "        # Put small JSON-serializable results here.\n"
        "    }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": result,\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n");
}

QString PythonCodeTemplates::functionFileToFileCode()
{
    return QStringLiteral(
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Read an upstream CSV file path and write a new CSV file artifact.\"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    source_path = Path(input_data.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "    output_path = artifact_dir / \"output.csv\"\n"
        "\n"
        "    with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "        # Read source_path and write transformed CSV content here.\n"
        "        # Example:\n"
        "        # import csv\n"
        "        # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as source_file:\n"
        "        #     reader = csv.DictReader(source_file)\n"
        "        #     writer = csv.DictWriter(file, fieldnames=reader.fieldnames or [])\n"
        "        #     writer.writeheader()\n"
        "        #     for row in reader:\n"
        "        #         writer.writerow(row)\n"
        "        pass\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(output_path),\n"
        "                \"source_file\": str(source_path),\n"
        "                \"format\": \"csv\",\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": \"csv\",\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\n"
        "                    \"source_file\": str(source_path),\n"
        "                    \"encoding\": \"utf-8\",\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                },\n"
        "            }\n"
        "        ],\n"
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
    return QStringLiteral("https://api.openai.com/v1");
}

QString PythonCodeTemplates::defaultAgentModel()
{
    return QStringLiteral("gpt-4o-mini");
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
    const QString& backgroundPrompt,
    const QString& taskPrompt,
    DataTransferTemplate transferTemplate)
{
    return QStringLiteral(
        "import json\n"
        "import urllib.error\n"
        "import urllib.request\n"
        "from pathlib import Path\n"
        "\n"
        "def run(inputs: dict, context: dict) -> dict:\n"
        "    \"\"\"Run an OpenAI-compatible chat completion as an Agent node.\"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    input_mode = %6\n"
        "    output_mode = %7\n"
        "\n"
        "    base_url = %1.strip()\n"
        "    model_name = %2.strip()\n"
        "    api_key = %3.strip()\n"
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
        "        source_path = Path(input_data.get(\"file_path\", \"\"))\n"
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
        "    request = urllib.request.Request(\n"
        "        endpoint,\n"
        "        data=json.dumps(payload, ensure_ascii=False).encode(\"utf-8\"),\n"
        "        headers={\"Content-Type\": \"application/json\", \"Authorization\": f\"Bearer {api_key}\"},\n"
        "        method=\"POST\",\n"
        "    )\n"
        "\n"
        "    try:\n"
        "        with urllib.request.urlopen(request, timeout=120) as response:\n"
        "            response_data = json.loads(response.read().decode(\"utf-8\"))\n"
        "    except urllib.error.HTTPError as exc:\n"
        "        body = exc.read().decode(\"utf-8\", errors=\"replace\")\n"
        "        raise RuntimeError(f\"Agent request failed: HTTP {exc.code}: {body}\") from exc\n"
        "\n"
        "    content = response_data[\"choices\"][0][\"message\"][\"content\"]\n"
        "    if output_mode == \"file\":\n"
        "        artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "        artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "        output_path = artifact_dir / \"agent_result.csv\"\n"
        "        # Ask the model for CSV content in the task prompt when this node should output a file.\n"
        "        output_path.write_text(content, encoding=\"utf-8\")\n"
        "        return {\n"
        "            \"outputs\": {\n"
        "                \"output\": {\n"
        "                    \"file_path\": str(output_path),\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                }\n"
        "            },\n"
        "            \"artifacts\": [\n"
        "                {\"type\": \"csv\", \"path\": str(output_path), \"metadata\": {\"encoding\": \"utf-8\", \"size_bytes\": output_path.stat().st_size}}\n"
        "            ],\n"
        "        }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"result\": content,\n"
        "                \"raw_response\": response_data,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [],\n"
        "    }\n")
        .arg(pythonStringLiteral(url),
            pythonStringLiteral(model),
            pythonStringLiteral(apiKey),
            pythonStringLiteral(backgroundPrompt),
            pythonStringLiteral(taskPrompt),
            pythonStringLiteral(agentInputMode(transferTemplate)),
            pythonStringLiteral(agentOutputMode(transferTemplate)));
}

QString PythonCodeTemplates::defaultAgentCode()
{
    return agentCode(
        defaultAgentUrl(),
        defaultAgentModel(),
        QString(),
        defaultAgentBackgroundPrompt(),
        defaultAgentTaskPrompt(),
        DataTransferTemplate::DataToData);
}

QString PythonCodeTemplates::defaultCodeForNodeType(const QString& nodeType)
{
    const auto normalized = nodeType.trimmed().toLower();
    if (normalized == "starter") {
        return starterDataOutputCode();
    }
    if (normalized == "agent") {
        return defaultAgentCode();
    }
    return functionDataToDataCode();
}

} // namespace vws::ui
