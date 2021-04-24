#include <QJsonDocument>
#include <QJsonObject>
#include "client.h"
#include "main.h"

namespace MtbNetLib {

DaemonClient::DaemonClient(QObject *parent) : QObject(parent) {
	QObject::connect(&this->m_tKeepAlive, SIGNAL(timeout()), this, SLOT(tKeepAliveTick()));
	QObject::connect(&m_socket, SIGNAL(connected()), this, SLOT(clientConnected()));
	QObject::connect(&m_socket, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
	QObject::connect(&m_socket, SIGNAL(readyRead()), this, SLOT(clientReadyRead()));
	QObject::connect(&m_socket, SIGNAL(errorOccured(QAbstractSocket::SocketError)),
	                 this, SLOT(clientErrorOccured(QAbstractSocket::SocketError)));
}

void DaemonClient::connect(const QHostAddress &addr, quint16 port, bool keepAlive) {
	this->m_socket.connectToHost(addr, port);
	if (keepAlive)
		this->m_tKeepAlive.start(CLIENT_KEEP_ALIVE_SEND_PERIOD_MS);
}

void DaemonClient::disconnect() {
	this->m_socket.abort();
}

bool DaemonClient::connected() const {
	return this->m_socket.state() == QAbstractSocket::SocketState::ConnectedState;
}

void DaemonClient::clientConnected() {
	onConnected();
}

void DaemonClient::clientDisconnected() {
	this->m_tKeepAlive.stop();
	// client->deleteLater();
	onDisconnected();
}

void DaemonClient::clientErrorOccured(QAbstractSocket::SocketError) {
	log("Daemon server socket error occured: "+m_socket.errorString(), LogLevel::Error);
	if (this->connected())
		this->disconnect();
	onDisconnected();
}

void DaemonClient::clientReadyRead() {
	if (this->m_socket.canReadLine()) {
		QByteArray data = this->m_socket.readLine();
		if (data.size() > 0) {
			QJsonObject json = QJsonDocument::fromJson(data).object();
			jsonReceived(json);
		}
	}
}

void DaemonClient::send(const QJsonObject &jsonObj) {
	if (!this->connected())
		return; // TODO: throw exception?
	this->m_socket.write(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
	this->m_socket.write("\n");
}

void DaemonClient::tKeepAliveTick() {
	if (this->connected())
		this->send({});
}

}; // namespace MtbNetLib
