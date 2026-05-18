#pragma once

#include <QString>

namespace vws::ui {

struct WorkflowGenerationDialogViewModel {
    QString url;
    QString modelName;
    QString presetPrompt;
    QString userRequirement;
    QString generatedJson;
    QString statusMessage;
    bool loading = false;
};

} // namespace vws::ui
