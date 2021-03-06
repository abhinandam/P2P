#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

typedef QMap<QString,QVariant> MsgMap;
typedef QMap<QString, quint32>::iterator wantMapIter;

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed();
    void displayText(QMap<QString, QVariant>);

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
    QByteArray serializeMessage(QString);
    void sendMessage(QByteArray);
    void sendToNeighbor(QByteArray);
    void checkStatus(QMap<QString, QMap<QString, quint32> >);
    void checkRumor(MsgMap);


public slots:
    void receiveMessage();
    void timeOut();
    void antiEntTimeOut();

private:
    int myPortMin, myPortMax;


};

#endif // P2PAPP_MAIN_HH
