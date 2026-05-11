#pragma once

#include <QCompleter>
#include <QStringList>

namespace vws::ui {

// PythonCompleter 只封装补全词源。
// 具体触发和插入由 PythonCodeEditor 控制。
class PythonCompleter final : public QCompleter {
public:
    explicit PythonCompleter(QObject* parent = nullptr);

    void refreshFromCode(const QString& code);

private:
    QStringList baseWords() const;
};

} // namespace vws::ui
