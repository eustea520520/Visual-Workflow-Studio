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
        "    \"\"\"Create a file artifact and pass its path downstream.\n"
        "\n"
        "    This template intentionally does not force a file type.\n"
        "    You can change file_name, file_type, and writing logic to produce any file type.\n"
        "    \"\"\"\n"
        "\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    file_name = \"output\"\n"
        "    file_type = \"\"\n"
        "    output_path = artifact_dir / file_name\n"
        "\n"
        "    output_path.write_text(\"\", encoding=\"utf-8\")\n"
        "\n"
        "    # You can use any file type:\n"
        "    # file_type = \"txt\"\n"
        "    # file_type = \"json\"\n"
        "    # file_type = \"md\"\n"
        "    # file_type = \"csv\"\n"
        "    # file_type = \"png\"\n"
        "    # file_type = \"pdf\"\n"
        "    # file_type = \"xlsx\"\n"
        "    #\n"
        "    # CSV example:\n"
        "    # file_name = \"output.csv\"\n"
        "    # file_type = \"csv\"\n"
        "    # output_path = artifact_dir / file_name\n"
        "    # import csv\n"
        "    # with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "    #     writer.writeheader()\n"
        "    #     writer.writerow({\"column_a\": \"value\", \"column_b\": 1})\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(output_path),\n"
        "                \"format\": file_type,\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": file_type,\n"
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
        "    \"\"\"Read small upstream business data and write a file artifact.\n"
        "\n"
        "    This template intentionally does not force a file type.\n"
        "    You can change file_name, file_type, and writing logic to produce any file type.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    file_name = \"output\"\n"
        "    file_type = \"\"\n"
        "    output_path = artifact_dir / file_name\n"
        "\n"
        "    # Write the file here. This default writes empty text.\n"
        "    output_path.write_text(\"\", encoding=\"utf-8\")\n"
        "\n"
        "    # CSV example:\n"
        "    # file_name = \"output.csv\"\n"
        "    # file_type = \"csv\"\n"
        "    # output_path = artifact_dir / file_name\n"
        "    # import csv\n"
        "    # with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     writer = csv.DictWriter(file, fieldnames=[\"column_a\", \"column_b\"])\n"
        "    #     writer.writeheader()\n"
        "    #     writer.writerow({\"column_a\": input_data.get(\"a\"), \"column_b\": input_data.get(\"b\")})\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(output_path),\n"
        "                \"format\": file_type,\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": file_type,\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\"size_bytes\": output_path.stat().st_size},\n"
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
        "    \"\"\"Read an upstream file path and return file metadata.\n"
        "\n"
        "    This template intentionally does not assume a file type.\n"
        "    Read the upstream file and extract the data you need.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    source_path = Path(input_data.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    source_format = input_data.get(\"format\", \"\")\n"
        "\n"
        "    result = {\n"
        "        \"file_path\": str(source_path),\n"
        "        \"format\": source_format,\n"
        "        \"size_bytes\": source_path.stat().st_size,\n"
        "        # Add derived data here.\n"
        "    }\n"
        "\n"
        "    # CSV example:\n"
        "    # import csv\n"
        "    # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #     reader = csv.DictReader(file)\n"
        "    #     rows = list(reader)\n"
        "    #     result[\"row_count\"] = len(rows)\n"
        "    #     result[\"columns\"] = reader.fieldnames\n"
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
        "    \"\"\"Read an upstream file and produce a new file artifact.\n"
        "\n"
        "    This template copies the source file as a simple default.\n"
        "    Replace the copy logic with any transformation you need.\n"
        "    \"\"\"\n"
        "    input_data = inputs.get(\"input\", {})\n"
        "    source_path = Path(input_data.get(\"file_path\", \"\"))\n"
        "    if not source_path.exists():\n"
        "        raise FileNotFoundError(f\"Input file does not exist: {source_path}\")\n"
        "\n"
        "    artifact_dir = Path(context.get(\"artifact_path\") or context.get(\"run_path\") or \".\")\n"
        "    artifact_dir.mkdir(parents=True, exist_ok=True)\n"
        "\n"
        "    file_name = \"output\"\n"
        "    file_type = \"\"\n"
        "    output_path = artifact_dir / file_name\n"
        "\n"
        "    # Copy the source file as a default. Replace with your own transformation.\n"
        "    output_path.write_bytes(source_path.read_bytes())\n"
        "\n"
        "    # CSV transformation example:\n"
        "    # file_name = \"output.csv\"\n"
        "    # file_type = \"csv\"\n"
        "    # output_path = artifact_dir / file_name\n"
        "    # import csv\n"
        "    # with source_path.open(\"r\", encoding=\"utf-8\", newline=\"\") as source_file:\n"
        "    #     reader = csv.DictReader(source_file)\n"
        "    #     with output_path.open(\"w\", encoding=\"utf-8\", newline=\"\") as file:\n"
        "    #         writer = csv.DictWriter(file, fieldnames=reader.fieldnames or [])\n"
        "    #         writer.writeheader()\n"
        "    #         for row in reader:\n"
        "    #             writer.writerow(row)\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"file_path\": str(output_path),\n"
        "                \"source_file\": str(source_path),\n"
        "                \"format\": file_type,\n"
        "                \"size_bytes\": output_path.stat().st_size,\n"
        "            }\n"
        "        },\n"
        "        \"artifacts\": [\n"
        "            {\n"
        "                \"type\": file_type,\n"
        "                \"path\": str(output_path),\n"
        "                \"metadata\": {\n"
        "                    \"source_file\": str(source_path),\n"
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
    return QStringLiteral(
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
        "    input_data = inputs.get(\"input\", {})\n"
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
        "        file_name = \"agent_result\"\n"
        "        file_type = \"\"\n"
        "        output_path = artifact_dir / file_name\n"
        "        # If you ask the model to return CSV, you may change:\n"
        "        # file_name = \"agent_result.csv\"\n"
        "        # file_type = \"csv\"\n"
        "        output_path.write_text(content, encoding=\"utf-8\")\n"
        "        return {\n"
        "            \"outputs\": {\n"
        "                \"output\": {\n"
        "                    \"file_path\": str(output_path),\n"
        "                    \"format\": file_type,\n"
        "                    \"size_bytes\": output_path.stat().st_size,\n"
        "                    \"retry_count\": retry_count,\n"
        "                    \"max_retries\": max_retries,\n"
        "                }\n"
        "            },\n"
        "            \"artifacts\": [\n"
        "                {\"type\": file_type, \"path\": str(output_path), \"metadata\": {\"size_bytes\": output_path.stat().st_size, \"retry_count\": retry_count, \"max_retries\": max_retries}}\n"
        "            ],\n"
        "        }\n"
        "\n"
        "    return {\n"
        "        \"outputs\": {\n"
        "            \"output\": {\n"
        "                \"result\": content,\n"
        "                \"raw_response\": response_data,\n"
        "                \"retry_count\": retry_count,\n"
        "                \"max_retries\": max_retries,\n"
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
    if (normalized == "starter") {
        return starterDataOutputCode();
    }
    if (normalized == "agent") {
        return defaultAgentCode();
    }
    return functionDataToDataCode();
}

} // namespace vws::ui
