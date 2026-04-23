#ifndef CORE85_GUI_FINDREPLACEBAR_H
#define CORE85_GUI_FINDREPLACEBAR_H

#include <QWidget>

class QCheckBox;
class QLineEdit;

namespace Core85::Gui {

class FindReplaceBar final : public QWidget {
    Q_OBJECT

public:
    explicit FindReplaceBar(QWidget* parent = nullptr);

    QString findText() const;
    QString replaceText() const;
    void focusFind();
    void focusGoToLine();
    void setFindText(const QString& text);

signals:
    void findNextRequested(const QString& text, bool caseSensitive, bool wholeWord);
    void findPreviousRequested(const QString& text, bool caseSensitive, bool wholeWord);
    void replaceOneRequested(const QString& findText,
                             const QString& replaceText,
                             bool caseSensitive,
                             bool wholeWord);
    void replaceAllRequested(const QString& findText,
                             const QString& replaceText,
                             bool caseSensitive,
                             bool wholeWord);
    void goToLineRequested(int line);
    void closeRequested();

private:
    QLineEdit* findInput_ = nullptr;
    QLineEdit* replaceInput_ = nullptr;
    QLineEdit* goToLineInput_ = nullptr;
    QCheckBox* caseSensitiveCheck_ = nullptr;
    QCheckBox* wholeWordCheck_ = nullptr;
};

}  // namespace Core85::Gui

#endif
