#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
    void displayText(QString);

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();
    void receiveMessage();
    void sendMessage(QByteArray);
    void serializeMessage(QString message);

private:
    int myPortMin, myPortMax;
    quint32 seqNum;
    QString origin;
    QMap<QString, QVariant> messageMap;

};

#endif // P2PAPP_MAIN_HH
