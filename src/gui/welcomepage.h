#ifndef CORE85_GUI_WELCOMEPAGE_H
#define CORE85_GUI_WELCOMEPAGE_H

#include <QWidget>

namespace Core85::Gui {

class WelcomePage final : public QWidget {
    Q_OBJECT

public:
    explicit WelcomePage(QWidget* parent = nullptr);

signals:
    void newFileRequested();
    void openFilesRequested();
    void openFolderRequested();
    void openSampleRequested();
};

}  // namespace Core85::Gui

#endif
