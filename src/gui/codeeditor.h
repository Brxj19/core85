#ifndef CORE85_GUI_CODEEDITOR_H
#define CORE85_GUI_CODEEDITOR_H

#include <QPlainTextEdit>
#include <QSet>

class QWidget;

namespace Core85::Gui {

class CodeEditor final : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    void lineNumberAreaMousePressEvent(QMouseEvent* event);
    void lineNumberAreaMouseMoveEvent(QMouseEvent* event);
    void lineNumberAreaLeaveEvent();

    QSet<int> breakpointLines() const;
    void setCurrentExecutionLine(int line);
    void setErrorLine(int line, const QString& message);
    void clearProblemMarkers();
    void setBreakpointLines(const QSet<int>& breakpoints);
    void goToLine(int line);

signals:
    void breakpointToggled(int line, bool enabled);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void refreshExtraSelections();

private:
    void toggleBreakpointForLine(int line);
    int lineFromY(int y) const;

    QWidget* lineNumberArea_ = nullptr;
    QSet<int> breakpoints_;
    int executionLine_ = -1;
    int errorLine_ = -1;
    int hoveredLine_ = -1;
    QString errorMessage_;
};

}  // namespace Core85::Gui

#endif
