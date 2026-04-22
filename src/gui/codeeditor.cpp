#include "gui/codeeditor.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>

namespace Core85::Gui {

namespace {

class AssemblySyntaxHighlighter final : public QSyntaxHighlighter {
public:
    explicit AssemblySyntaxHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent) {
        QTextCharFormat opcodeFormat;
        opcodeFormat.setForeground(QColor(QStringLiteral("#2F5AA8")));
        opcodeFormat.setFontWeight(QFont::Bold);
        addRule(opcodePattern(), opcodeFormat);

        QTextCharFormat registerFormat;
        registerFormat.setForeground(QColor(QStringLiteral("#0F766E")));
        addRule(QStringLiteral("\\b(A|B|C|D|E|H|L|M|SP|PSW|BC|DE|HL)\\b"), registerFormat);

        QTextCharFormat numberFormat;
        numberFormat.setForeground(QColor(QStringLiteral("#C66A00")));
        addRule(QStringLiteral("\\b(0X[0-9A-F]+|[0-9A-F]+H|[0-9]+D?|'.')\\b"), numberFormat);

        QTextCharFormat directiveFormat;
        directiveFormat.setForeground(QColor(QStringLiteral("#7C3AED")));
        addRule(QStringLiteral("\\b(ORG|EQU|DB|DW|END|\\.ORG|\\.EQU)\\b"), directiveFormat);

        QTextCharFormat labelFormat;
        labelFormat.setForeground(QColor(QStringLiteral("#1D7A3F")));
        addRule(QStringLiteral("^\\s*[A-Z_][A-Z0-9_]*:"), labelFormat);

        commentFormat_.setForeground(QColor(QStringLiteral("#6B7280")));
        commentFormat_.setFontItalic(true);
    }

protected:
    void highlightBlock(const QString& text) override {
        const QString upper = text.toUpper();
        for (const auto& rule : rules_) {
            auto match = rule.pattern.globalMatch(upper);
            while (match.hasNext()) {
                const auto token = match.next();
                setFormat(token.capturedStart(), token.capturedLength(), rule.format);
            }
        }

        const int commentIndex = text.indexOf(QLatin1Char(';'));
        if (commentIndex >= 0) {
            setFormat(commentIndex, text.size() - commentIndex, commentFormat_);
        }
    }

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    static QString opcodePattern() {
        return QStringLiteral(
            "\\b(ACI|ADC|ADD|ADI|ANA|ANI|CALL|CC|CM|CMA|CMC|CMP|CNC|CNZ|CP|CPE|CPI|CPO|CZ|DAA|DAD|DCR|DCX|DI|EI|HLT|IN|INR|INX|JC|JM|JMP|JNC|JNZ|JP|JPE|JPO|JZ|LDA|LDAX|LHLD|LXI|MOV|MVI|NOP|ORA|ORI|OUT|PCHL|POP|PUSH|RAL|RAR|RC|RET|RIM|RLC|RM|RNC|RNZ|RP|RPE|RPO|RRC|RST|RZ|SBB|SBI|SHLD|SIM|SPHL|STA|STAX|STC|SUB|SUI|XCHG|XRA|XRI|XTHL)\\b");
    }

    void addRule(const QString& pattern, const QTextCharFormat& format) {
        rules_.push_back(HighlightRule{QRegularExpression(pattern), format});
    }

    QVector<HighlightRule> rules_{};
    QTextCharFormat commentFormat_{};
};

class CodeEditorLineNumberArea final : public QWidget {
public:
    explicit CodeEditorLineNumberArea(CodeEditor* editor)
        : QWidget(editor), editor_(editor) {
    }

    QSize sizeHint() const override {
        return QSize(editor_->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        editor_->lineNumberAreaPaintEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        editor_->lineNumberAreaMousePressEvent(event);
    }

private:
    CodeEditor* editor_;
};

}  // namespace

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent), lineNumberArea_(new CodeEditorLineNumberArea(this)) {
    auto* highlighter = new AssemblySyntaxHighlighter(document());
    Q_UNUSED(highlighter);

    QFont editorFont(QStringLiteral("Menlo"));
    editorFont.setStyleHint(QFont::Monospace);
    editorFont.setPointSize(12);
    setFont(editorFont);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::refreshExtraSelections);
    updateLineNumberAreaWidth(0);
    refreshExtraSelections();
}

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    return 24 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    const bool darkTheme = palette().base().color().lightness() < 128;
    painter.fillRect(event->rect(),
                     darkTheme ? QColor(QStringLiteral("#172033"))
                               : QColor(QStringLiteral("#EEF2F7")));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int lineNumber = blockNumber + 1;
            painter.setPen(darkTheme ? QColor(QStringLiteral("#94A3B8"))
                                     : QColor(QStringLiteral("#667085")));
            painter.drawText(0,
                             top,
                             lineNumberArea_->width() - 12,
                             fontMetrics().height(),
                             Qt::AlignRight,
                             QString::number(lineNumber));

            if (breakpoints_.contains(lineNumber)) {
                painter.setBrush(QColor(QStringLiteral("#DC2626")));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(4, top + 3, 8, 8);
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::lineNumberAreaMousePressEvent(QMouseEvent* event) {
    const int line = lineFromY(event->pos().y());
    if (line > 0) {
        toggleBreakpointForLine(line);
    }
}

QSet<int> CodeEditor::breakpointLines() const {
    return breakpoints_;
}

void CodeEditor::setCurrentExecutionLine(int line) {
    executionLine_ = line;
    refreshExtraSelections();
}

void CodeEditor::setErrorLine(int line, const QString& message) {
    errorLine_ = line;
    errorMessage_ = message;
    refreshExtraSelections();
}

void CodeEditor::clearProblemMarkers() {
    errorLine_ = -1;
    errorMessage_.clear();
    executionLine_ = -1;
    refreshExtraSelections();
}

void CodeEditor::setBreakpointLines(const QSet<int>& breakpoints) {
    breakpoints_ = breakpoints;
    lineNumberArea_->update();
}

void CodeEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    const QRect contents = contentsRect();
    lineNumberArea_->setGeometry(
        QRect(contents.left(), contents.top(), lineNumberAreaWidth(), contents.height()));
}

void CodeEditor::updateLineNumberAreaWidth(int newBlockCount) {
    Q_UNUSED(newBlockCount);
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy != 0) {
        lineNumberArea_->scroll(0, dy);
    } else {
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::refreshExtraSelections() {
    QList<QTextEdit::ExtraSelection> selections;
    const bool darkTheme = palette().base().color().lightness() < 128;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection currentLineSelection;
        currentLineSelection.format.setBackground(
            darkTheme ? QColor(QStringLiteral("#1E3A5F"))
                      : QColor(QStringLiteral("#EEF4FF")));
        currentLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        currentLineSelection.cursor = textCursor();
        currentLineSelection.cursor.clearSelection();
        selections.push_back(currentLineSelection);
    }

    auto addLineSelection = [&](int line, const QColor& color) {
        if (line <= 0) {
            return;
        }

        QTextBlock block = document()->findBlockByNumber(line - 1);
        if (!block.isValid()) {
            return;
        }

        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(color);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = QTextCursor(block);
        selection.cursor.clearSelection();
        selections.push_back(selection);
    };

    addLineSelection(executionLine_,
                     darkTheme ? QColor(QStringLiteral("#6B5A12"))
                               : QColor(QStringLiteral("#FEF08A")));
    addLineSelection(errorLine_,
                     darkTheme ? QColor(QStringLiteral("#7F1D1D"))
                               : QColor(QStringLiteral("#FECACA")));

    setExtraSelections(selections);
}

void CodeEditor::toggleBreakpointForLine(int line) {
    if (breakpoints_.contains(line)) {
        breakpoints_.remove(line);
        emit breakpointToggled(line, false);
    } else {
        breakpoints_.insert(line);
        emit breakpointToggled(line, true);
    }
    lineNumberArea_->update();
}

int CodeEditor::lineFromY(int y) const {
    QTextBlock block = firstVisibleBlock();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid()) {
        if (y >= top && y < bottom) {
            return block.blockNumber() + 1;
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
    }

    return -1;
}

}  // namespace Core85::Gui
