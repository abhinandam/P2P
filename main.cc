
#include <unistd.h>
#include <iostream>
#include <string>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QMap>
#include <QVariant>
#include <QDataStream>

#include "main.hh"

ChatDialog *chatDialog;
NetSocket *socket;

int origin;
quint32 seqNum;

ChatDialog::ChatDialog() {
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

void ChatDialog::gotReturnPressed() {
	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() << "FIX: send message to other peers: " << textline->text();
    QString displayText = QString::number(origin) +": " + textline->text();
    textview->append(displayText);

    QString message = textline->text();
    socket->serializeMessage(message);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::displayText(QMap<QString, QVariant> inputMap) {
    // don't repeat print to sender since we printed on gotReturnPressed()
    if (inputMap.contains("ChatText") && (inputMap.contains("Origin"))) {
        if (QString::number(origin) != inputMap["Origin"].toString()) {
            QString message_string = inputMap["ChatText"].toString();
            QString origin_string = inputMap["Origin"].toString();
            QString s = origin_string + ": " + message_string;
            textview->append(s);
        }
    }
}

NetSocket::NetSocket() {
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}


void NetSocket::serializeMessage(QString message) {
    QMap<QString, QVariant> map;
    seqNum++;
    map.insert("ChatText", QVariant(message));
    map.insert("Origin", QVariant(origin));
    map.insert("SeqNum", QVariant(seqNum));

    QByteArray serializedMap;
    QDataStream * dataStream = new QDataStream(&serializedMap, QIODevice::WriteOnly);
    (*dataStream) << map;

    for(int i = myPortMin; i <= myPortMax; i++) {
        socket->writeDatagram(serializedMap, QHostAddress::LocalHost, i);
    }

    delete dataStream;
}

void NetSocket::receiveMessage() {
    QByteArray datagram;
    QHostAddress sender;
    u_int16_t port;

    while(socket->hasPendingDatagrams()) {
        datagram.resize(pendingDatagramSize());
        readDatagram(datagram.data(), datagram.size(), &sender, &port);

        // convert ByteArray to map
        QMap<QString, QVariant> inputMap;
        QDataStream dataStream(&datagram, QIODevice::ReadOnly);
        dataStream >> inputMap;

        chatDialog->displayText(inputMap);
    }
}


bool NetSocket::bind() {
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
            origin = p;
			qDebug() << "bound to UDP port " << p;
            connect(this, SIGNAL(readyRead()), this, SLOT(receiveMessage()));
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

int main(int argc, char **argv) {
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	NetSocket sock;
	if (!sock.bind())
		exit(1);

	// set up some env vars
    socket = &sock;
    chatDialog = &dialog;

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
