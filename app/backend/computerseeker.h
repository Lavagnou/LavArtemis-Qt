#pragma once

#include <QObject>

class ComputerManager;
class NvComputer;
class QTimer;

class ComputerSeeker : public QObject
{
    Q_OBJECT
public:
    explicit ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent = nullptr);

    void start(int timeout);

    // Why the search ended without a computer, when it ended before its timeout (test build: the
    // address is refused by TestGuard). Empty for a plain timeout.
    QString errorMessage() const { return m_ErrorMessage; }

signals:
    void computerFound(NvComputer *computer);
    void errorTimeout();

private slots:
    void onComputerUpdated(NvComputer *computer);
    void onTimeout();

private:
    bool matchComputer(NvComputer *computer) const;
    NvComputer* findMatchingComputer() const;
    bool isOnline(NvComputer *computer) const;

private:
    ComputerManager *m_ComputerManager;
    QString m_ComputerName;
    QTimer *m_TimeoutTimer;
    QString m_ErrorMessage;
};
