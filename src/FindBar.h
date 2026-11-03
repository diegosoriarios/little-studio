#ifndef LITE_STUDIO_FIND_BAR_H
#define LITE_STUDIO_FIND_BAR_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;

// Slim find/replace bar shown above the editor tabs.
// Pure UI: emits signals, MainWindow owns the search logic.
class FindBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent = nullptr);

    QString searchText() const;
    QString replaceText() const;
    bool matchCase() const;
    bool wholeWord() const;

    void setSearchText(const QString &text);
    void setMatchCase(bool on);
    void setWholeWord(bool on);
    void showMessage(const QString &text);
    void focusSearchField();

signals:
    void findNextRequested();
    void findPreviousRequested();
    void replaceRequested();
    void replaceAllRequested();
    void optionsChanged();
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *m_searchField = nullptr;
    QLineEdit *m_replaceField = nullptr;
    QCheckBox *m_caseBox = nullptr;
    QCheckBox *m_wordBox = nullptr;
    QLabel *m_status = nullptr;
};

#endif
