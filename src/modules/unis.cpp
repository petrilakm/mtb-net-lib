#include <QJsonArray>
#include <QJsonObject>
#include "unis.h"
#include "../errors.h"
#include "../events.h"
#include "../main.h"

namespace MtbNetLib {

/* Daemon events ------------------------------------------------------------ */

void MtbUnis::daemonGotInfo(const QJsonObject& json) {
	QString oldState = this->state;
	MtbModule::daemonGotInfo(json);
	const QJsonObject& uniJson = json[this->type_str].toObject();
        //this->ir = uniJson["ir"].toBool();
	if (uniJson.contains("config"))
		this->config = uniJson["config"].toObject();
	if (oldState == "")
		this->resetOutputsState();
	if ((oldState != "active") && (oldState != "") && (this->state == "active")) {
		// Restore state of outputs
		this->restoreOutputs();
	}
	if (uniJson.contains("state")) {
		const QJsonObject& state = uniJson["state"].toObject();
		const QJsonObject& outputs = state["outputs"].toObject();
		for (const auto& key : outputs.keys())
			this->outputsConfirmed[key.toInt()] = outputs[key].toObject();
		this->inputs = state["inputsPacked"].toInt();
	}

	// Must be here after setting all the attributes,
	// because hJOP can directly ask for module information
	events.call(events.onModuleChanged, this->address);

	if (MtbNetLib::state.rcs >= MtbNetLib::RcsState::stopped) {
		if ((oldState != "active") && (this->state == "active")) {
			events.call(events.onError, RCS_MODULE_RESTORED, this->address, "Module activated");
			events.call(events.onInputChanged, this->address);
			events.call(events.onOutputChanged, this->address);
		}
		if ((oldState == "active") && (this->state != "active")) {
			events.call(events.onError, RCS_MODULE_FAILED, this->address, "Module failed");
			events.call(events.onInputChanged, this->address);
			events.call(events.onOutputChanged, this->address);
		}
	}
}

void MtbUnis::daemonInputsChanged(const QJsonObject& json) {
	this->inputs = json["packed"].toInt();
	events.call(events.onInputChanged, this->address);
}

void MtbUnis::daemonOutputsChanged(const QJsonObject& json) {
	for (const auto& key : json.keys())
		this->outputsConfirmed[key.toInt()] = json[key].toObject();
	events.call(events.onOutputChanged, this->address);
}

void MtbUnis::daemonOutputsSet(const QJsonObject& json) {
	for (const auto& key : json.keys())
		this->outputsConfirmed[key.toInt()] = json[key].toObject();
	events.call(events.onOutputChanged, this->address);
}

/* RCS events --------------------------------------------------------------- */

int MtbUnis::rcsGetInput(unsigned int port) {
	if (port > UNIS_IN_CNT)
		return RCS_PORT_INVALID_NUMBER;
	if (this->state != "active")
		return RCS_MODULE_FAILED;

	return (this->inputs >> port) & 0x1;
}

int MtbUnis::rcsGetOutput(unsigned int port) {
	if (port > UNIS_OUT_CNT)
		return RCS_PORT_INVALID_NUMBER;
	if (this->state != "active")
		return RCS_MODULE_FAILED;

	return this->outputsConfirmed[port]["value"].toInt();
}

int MtbUnis::rcsSetOutput(unsigned int port, int state) {
	if (port > UNIS_OUT_CNT)
		return RCS_PORT_INVALID_NUMBER;
	if (this->state != "active")
		return RCS_MODULE_FAILED;

	/* This code is intentionally commented-out!
	 * One cannot check state against confirmed state, because there can be
	 * SetOutput pending!
	 * if (this->outputsConfirmed[port]["value"].toInt() == state)
	 *     return 0;
	 */

	QJsonArray outputsSafe = this->config["outputsSafe"].toArray();
	if (static_cast<int>(port) >= outputsSafe.size())
		return RCS_PORT_INVALID_NUMBER;

	QString portType = outputsSafe[port].toObject()["type"].toString();
	if ((state > 1) && (portType == "plain"))
		portType = "flicker";

	daemonClient.send(QJsonObject{
		{"command", "module_set_outputs"},
		{"type", "request"},
		{"address", this->address},
		{"outputs", QJsonObject{
			{QString::number(port), QJsonObject{
				{"type", portType},
				{"value", state},
			}},
		}},
	});

	return 0;
}

int MtbUnis::rcsGetInputType(unsigned int port) {
	if (port > UNIS_IN_CNT)
		return RCS_PORT_INVALID_NUMBER;

	if (this->config.contains("irsPacked")) {
		uint16_t irs = this->config["irsPacked"].toInt();
		irs >>= port;
		if (irs & 1)
			return static_cast<int>(RcsPortInputType::iIr);
	}
	return static_cast<int>(RcsPortInputType::iPlain);
}

int MtbUnis::rcsGetOutputType(unsigned int port) {
	if (port > UNIS_OUT_CNT)
		return RCS_PORT_INVALID_NUMBER;

	QJsonArray outputsSafe = this->config["outputsSafe"].toArray();
	if (static_cast<int>(port) >= outputsSafe.size())
		return RCS_PORT_INVALID_NUMBER;

	const QString& portType = outputsSafe[port].toObject()["type"].toString();
	if (portType == "s-com")
		return static_cast<int>(RcsPortOutputType::oScom);
	return static_cast<int>(RcsPortOutputType::oPlain);
}

void MtbUnis::resetConfig() {
	this->config = {};
}

void MtbUnis::resetOutputsState() {
	const QJsonArray& safeState = this->config["outputsSafe"].toArray();
	for (size_t i = 0; i < std::min<size_t>(UNIS_OUT_CNT, safeState.size()); i++)
		this->outputsConfirmed[i] = safeState[i].toObject();
}

void MtbUnis::resetInputsState() {
	this->inputs = 0;
}

void MtbUnis::restoreOutputs() const {
	QJsonObject outputs;
	for (size_t i = 0; i < UNIS_OUT_CNT; i++)
		outputs[QString::number(i)] = this->outputsConfirmed[i];

	daemonClient.send(QJsonObject{
		{"command", "module_set_outputs"},
		{"type", "request"},
		{"address", this->address},
		{"outputs", outputs},
	});
}

size_t MtbUnis::inputsCount() const { return UNIS_IN_CNT; }
size_t MtbUnis::outputsCount() const { return UNIS_OUT_CNT; }

} // namespace MtbNetLib
