#include "ui/editor/PythonCompleter.h"

#include <QRegularExpression>
#include <QStringListModel>
#include <QSet>

namespace vws::ui {

PythonCompleter::PythonCompleter(QObject* parent)
    : QCompleter(parent)
{
    setCaseSensitivity(Qt::CaseInsensitive);
    setCompletionMode(QCompleter::PopupCompletion);
    setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    refreshFromCode({});
}

void PythonCompleter::refreshFromCode(const QString& code)
{
    QSet<QString> words;
    for (const auto& word : baseWords()) {
        words.insert(word);
    }
    auto iterator = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*\\b").globalMatch(code);
    while (iterator.hasNext()) {
        const auto word = iterator.next().captured();
        if (word.size() >= 2) {
            words.insert(word);
        }
    }

    QStringList sortedWords(words.begin(), words.end());
    sortedWords.sort(Qt::CaseInsensitive);
    setModel(new QStringListModel(sortedWords, this));
}

QStringList PythonCompleter::baseWords() const
{
    return {
        "def", "class", "if", "else", "elif", "for", "while", "try", "except", "finally",
        "return", "import", "from", "as", "with", "lambda", "yield", "in", "is", "and", "or",
        "not", "None", "True", "False",
        "print", "len", "range", "dict", "list", "set", "tuple", "str", "int", "float",
        "open", "sum", "min", "max", "abs", "enumerate", "zip",
        "inputs", "context", "outputs", "artifacts",
    };
}

} // namespace vws::ui
