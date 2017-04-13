
#include <unistd.h>
#include <iostream>
#include <string>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QTimer>


#include "main.hh"

ChatDialog *chatDialog;
NetSocket *socket;

// keep track of ports to send data to
int origin;
int resendPort;

// origin port num mapped to message QMap for all prev messages
QMap<QString, QMap<quint32, MsgMap> > messageDigest;
QMap<QString, quint32> wantMap;
QByteArray resendMessage;

quint32 seqNum;

// timers to verify status of messages across nodes
QTimer timer;
QTimer antiEntTimer;

ChatDialog::ChatDialog() {
	setWindowTitle("P2Papp");

	textview = new QTextEdit(this);
	textview->setReadOnly(true);
	textline = new QLineEdit(this);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::gotReturnPressed() {
    QString displayText = QString::number(origin) +": " + textline->text();
    textview->append(displayText);

    QString message = textline->text();

    // serialize and send
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
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;

    connect(this, SIGNAL(readyRead()), this, SLOT(receiveMessage()));

    // timer and anti-entropy timer used to ensure all dialogs get messages
    connect(&timer, SIGNAL(timeout()), this, SLOT(timeOut()));
    connect(&antiEntTimer, SIGNAL(timeout()), this, SLOT(antiEntTimeOut()));
    antiEntTimer.start(3000);

}


QByteArray NetSocket::serializeMessage(QString message) {
    // initialize message map for serialization
    MsgMap map;
    map.insert("ChatText", QVariant(message));
    map.insert("Origin", QVariant(origin));
    map.insert("SeqNum", QVariant(seqNum++));

    // add current message to origin port, or create new map if not existing
    QString originString = QString::number(origin);
    if(messageDigest.contains(originString)) {
        messageDigest[originString].insert(map["SeqNum"].toUInt(), map);
    } else { // haven't seen this origin yet -- new node conn
        QMap<quint32, MsgMap> newMessage;
        messageDigest.insert(originString, newMessage);
        messageDigest[originString].insert(map["SeqNum"].toUInt(), map);
    }

    // update wantMap to reflect next desired seqNum for given origin
    if (wantMap.contains(originString)) {
        (wantMap[originString]++);
    } else {
        (wantMap.insert(originString, 1));
    }


    // serialize Map
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
    // randomly choose neighbor if not min or max
    else {
        if(rand() % 2) {
            neighbor = origin + 1;
        }
        else {
            neighbor = origin - 1;
        }
    }
    socket->writeDatagram(message, QHostAddress::LocalHost, neighbor);
    resendPort = neighbor;
}

void NetSocket::receiveMessage() {
    QByteArray datagram;
    QHostAddress sender;
    u_int16_t port;

    while(socket->hasPendingDatagrams()) {
        datagram.resize(pendingDatagramSize());
        readDatagram(datagram.data(), datagram.size(), &sender, &port);
        resendPort = port;

        // convert ByteArray to map
        MsgMap inputMap;
        QDataStream dataStream(&datagram, QIODevice::ReadOnly);
        dataStream >> inputMap;

        // check status if map has "want", else rumor message
        if(inputMap.contains("Want")) {
            QMap<QString, QMap<QString, quint32> > wantMapTemp;
            QDataStream dataStream(&datagram, QIODevice::ReadOnly);
            dataStream >> wantMapTemp;
            timer.stop();
            checkStatus(wantMapTemp);
        } else {
            checkRumor(inputMap);
        }

        chatDialog->displayText(inputMap);
    }
}

void NetSocket::checkRumor(MsgMap message) {
    QString originId = message["Origin"].toString();
    quint32 seqNumRecv = message["SeqNum"].toUInt();
    bool isNewMsg = false;

    if(QString::number(origin) != originId) {
        // update seqNum for previously seen originId
        if(wantMap.contains(originId)) {
            if(seqNumRecv == wantMap[originId]) {
                isNewMsg = true;
            }
            wantMap[originId] = ++seqNumRecv;
        }
        else {
            isNewMsg = true;
            wantMap.insert(originId, ++seqNumRecv);
        }
    }
    else {
        // update seqNum for previously seen originId
        if(wantMap.contains(originId)) {
            wantMap[originId] = ++seqNumRecv;
        }
        else {
            wantMap.insert(originId, ++seqNumRecv);
        }
    }
    if (isNewMsg) {
        QString messageString = message["ChatText"].toString();
        // add to same origin Port if already seen
        if(messageDigest.contains(originId)) {
            messageDigest[originId].insert(seqNumRecv, message);
        } else {
            QMap<quint32, MsgMap> newMessage;
            messageDigest.insert(originId, newMessage);
            messageDigest[originId].insert(seqNumRecv, message);
        }
        QDataStream * dataStream = new QDataStream(&resendMessage, QIODevice::WriteOnly);
        (*dataStream) << message;
        delete dataStream;
    }
}


void NetSocket::checkStatus(QMap<QString, QMap<QString, quint32> > inputWantMap) {
    bool updated = true;

    QMap<QString, quint32> recvdWantMap = inputWantMap["Want"];
    MsgMap rumor;
    for(wantMapIter i = wantMap.begin(); i != wantMap.end(); i++) {
        QString originId = i.key();
        quint32 seqNo = wantMap[originId];
        if(recvdWantMap.contains(originId)) {
            if(recvdWantMap[originId] != seqNo) {
                updated = false;
                if(recvdWantMap[originId] < seqNo) {
                    // sending peer is behind remote peer, update rumor to send
                    rumor = messageDigest[originId][seqNo];
                }

            }
        } else {
            // send all rumors, since nothing contained in map
            updated = false;
            rumor = messageDigest[originId][0];
        }
    }

    // if not up to date, reserialize and send rumor to origin port
    if(!updated) {
        QByteArray serializedMessage;
        QDataStream * dataStream = new QDataStream(&serializedMessage, QIODevice::WriteOnly);
        (*dataStream) << rumor;
        socket->writeDatagram(serializedMessage, QHostAddress::LocalHost, origin);
        delete dataStream;
    }

}

void NetSocket::timeOut() {
    socket->writeDatagram(resendMessage, QHostAddress::LocalHost, resendPort);
    timer.start(2000);
}

void NetSocket::antiEntTimeOut() {
    // send status message to random neighbor and start timer
    QMap<QString, QMap<QString, quint32> > status;
    status.insert("Want", wantMap);

    QByteArray serializedStatus;
    QDataStream *dataStream = new QDataStream(&serializedStatus, QIODevice::WriteOnly);
    (*dataStream) << status;
    delete dataStream;

    sendToNeighbor(serializedStatus);
    antiEntTimer.start(10000);
}

bool NetSocket::bind() {
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
            origin = p;
			qDebug() << "bound to UDP port " << p;
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
