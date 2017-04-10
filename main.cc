
#include <unistd.h>
#include <iostream>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog * chatDialog;
NetSocket * socket;

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() << "FIX: send message to other peers: " << textline->text();
    QString message = textline->text();
    socket->serializeMessage(message);

    textview->append(QString("Port Num: ") + textline->text());

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::displayText(QString messageText) {
        qDebug() << "here5";
        //textview->append(messageText);
        textview->append("works");
        qDebug() << "here6";
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
    seqNum = 0;
    origin = "";
}

void NetSocket::serializeMessage(QString message) {
    QMap<QString, QVariant> map;
    map.insert("ChatText", QVariant(message));
    map.insert("Origin", QVariant(origin));
    map.insert("SeqNum", QVariant(seqNum));
    seqNum++;

    qDebug() << "here";
    QByteArray serializedMap;
    QDataStream * dataStream = new QDataStream(&serializedMap, QIODevice::WriteOnly);
        qDebug() << "here2";
    (*dataStream) << serializedMap;
    delete dataStream;

    sendMessage(serializedMap);
}

void NetSocket::sendMessage(QByteArray message) {

        qDebug() << "here3";
    for(int i = myPortMin; i <= myPortMax; i++) {
        writeDatagram(message, QHostAddress::LocalHost, i);
    }
        qDebug() << "here4";

    chatDialog->displayText(QString("test message"));

}

void NetSocket::receiveMessage() {
    QByteArray datagram;
    QHostAddress sender;
    u_int16_t port;

    while(socket->hasPendingDatagrams()) {
        datagram.resize(socket->pendingDatagramSize());
        readDatagram(datagram.data(), datagram.size(), &sender, &port);
        QMap<QString, QVariant> messageMap;
        QDataStream dataStream(&datagram, QIODevice::ReadOnly);
        dataStream >> messageMap;

        QString message = messageMap.value("ChatText").toString();
        QString origin = messageMap.value("Origin").toString();
        quint32 seqNum = messageMap.value("SeqNum").toUInt();

        chatDialog->displayText(QString("test message"));
    }
}


bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
            connect(this, SIGNAL(readyRead()), this, SLOT(receiveMessage()));
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	NetSocket sock;
	if (!sock.bind())
		exit(1);

    socket = &sock;


	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

