#ifndef MODULE_MTB_UNI_H
#define MODULE_MTB_UNI_H

#include "module.h"

namespace MtbNetLib {

constexpr size_t UNI_IO_CNT = 16;

class MtbUni : public MtbModule {
public:
	uint16_t inputs;
	std::array<QJsonObject, UNI_IO_CNT> outputsConfirmed;
	QJsonObject config;
	bool ir;

	MtbUni(uint8_t addr);
	~MtbUni() override = default;

	void daemonGotInfo(const QJsonObject&) override;
	void daemonInputsChanged(const QJsonObject&) override;
	void daemonOutputsChanged(const QJsonObject&) override;
	void daemonOutputsSet(const QJsonObject&) override;

	int rcsGetInput(unsigned int port) override;
	int rcsGetOutput(unsigned int port) override;
	int rcsSetOutput(unsigned int port, int state) override;
	int rcsGetInputType(unsigned int port) override;
	int rcsGetOutputsType(unsigned int port) override;

	void resetConfig() override;
	void resetState() override;
};

}; // namespace MtbNetLib

#endif
