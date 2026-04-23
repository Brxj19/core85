#ifndef CORE85_GUI_PROBLEMSPANEL_H
#define CORE85_GUI_PROBLEMSPANEL_H

#include <QWidget>

#include "core/assembler.h"

class QLabel;
class QTreeWidget;

namespace Core85::Gui {

class ProblemsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);

    void setProblems(const std::vector<Core85::AssemblerError>& problems);
    void clearProblems();
    bool hasProblems() const;

signals:
    void problemActivated(int line);

private:
    QString friendlyMessage(const Core85::AssemblerError& problem) const;

    QTreeWidget* tree_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
};

}  // namespace Core85::Gui

#endif
