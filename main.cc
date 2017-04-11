
#include <unistd.h>
#include <iostream>
#include <string>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>


#include "main.hh"

ChatDialog *chatDialog;
NetSocket *socket;

int origin;
int currPort;
int resendPort;

QMap<QString, QVariant> wantMap;
QMap<QString, quint32> log;

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
    QString displayText = QString::number(origin) +": " + textline->text();
    textview->append(displayText);

    QString message = textline->text();

    // serialized and send
    QByteArray rumor = socket->serializeMessage(message);
    socket->sendToNeighbor(rumor);

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


QByteArray NetSocket::serializeMessage(QString message) {
    QMap<QString, QVariant> map;
    map.insert("ChatText", QVariant(message));
    map.insert("Origin", QVariant(origin));
    map.insert("SeqNum", QVariant(seqNum++));

    QByteArray serializedMap;
    QDataStream * dataStream = new QDataStream(&serializedMap, QIODevice::WriteOnly);
    (*dataStream) << map;

    delete dataStream;
    return serializedMap;
}

void NetSocket::sendToNeighbor(QByteArray message) {

    int neighbor;
    if(origin == myPortMin) {
        neighbor = origin + 1;
    } else if (origin == myPortMax) {
         neighbor = origin - 1;
    }
    // randomly choose neighbor
    else {
        if(rand() % 2) {
            neighbor = origin + 1;
        }
        else {
            neighbor = origin - 1;
        }
    }
    socket->writeDatagram(message, QHostAddress::LocalHost, neighbor);
    qDebug() << QString("neighbor number " + QString::number(neighbor));
    resendPort = neighbor;
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
    srand(time(0));

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
