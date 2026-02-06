#include "XmlConfig.h"
#include "tinyxml/tinystr.h"
#include "tinyxml/tinyxml.h"
#include <time.h>

XmlConfig::XmlConfig(void)
{
	memset(&deviceConfig, 0x00, sizeof(XML_CONFIG_T));
}


XmlConfig::~XmlConfig(void)
{
}


int		XmlConfig::LoadConfig(const char *filename, XML_CONFIG_T *pConfig)
{
	if (NULL == filename)			return -1;

	memset(&deviceConfig, 0x00, sizeof(XML_CONFIG_T));

	int ret = -1;
	TiXmlDocument m_DocR;
	if (!m_DocR.LoadFile(filename))
	{
		//libLogger_Print(NULL, LOG_TYPE_DEBUG, "XmlConfig:: Failed to read configuration file.\nuse default parameters. file:%s\n", filename);

		char szTime[64] = { 0 };
		time_t tt = time(NULL);
		struct tm* _timetmp = NULL;
		_timetmp = localtime(&tt);
		if (NULL != _timetmp)   strftime(szTime, 32, "%m%d%H%M%S", _timetmp);

		strcpy(deviceConfig.serverIP, (char*)"rts.easyrtc.cn");
		deviceConfig.serverPort = 19000;
		deviceConfig.ssl = 0;
		sprintf(deviceConfig.deviceID, (char*)"92092eea-be8d-4ec4-8ac5-00%s", szTime);
		strcpy(deviceConfig.sourceURL, (char*)"rtsp://");

		SaveConfig(filename, &deviceConfig);			//不存在配置文件, 生成一个新的配置文件
		if (NULL != pConfig)		memcpy(pConfig, &deviceConfig, sizeof(XML_CONFIG_T));
		return ret;
	}

	//libLogger_Print(NULL, LOG_TYPE_DEBUG, "XmlConfig::Open the configuration file successfully: %s\n", filename);

	TiXmlHandle hDoc(&m_DocR);
	TiXmlHandle hRoot(0);

	TiXmlElement *pServerConfigXML = hDoc.FirstChild("XmlConfig").ToElement();
	if (NULL != pServerConfigXML)
	{
		TiXmlElement *pE;

		pE = pServerConfigXML->FirstChildElement("ServerIP");
		if (pE && pE->GetText())		strcpy(deviceConfig.serverIP, pE->GetText());


		pE = pServerConfigXML->FirstChildElement("ServerPort");
		if (pE && pE->GetText())		deviceConfig.serverPort = atoi(pE->GetText());

		pE = pServerConfigXML->FirstChildElement("SSL");
		if (pE && pE->GetText())		deviceConfig.ssl = atoi(pE->GetText());


		pE = pServerConfigXML->FirstChildElement("DeviceID");
		if (pE && pE->GetText())		strcpy(deviceConfig.deviceID, pE->GetText());

		pE = pServerConfigXML->FirstChildElement("SourceURL");
		if (pE && pE->GetText())		strcpy(deviceConfig.sourceURL, pE->GetText());

		if (NULL != pConfig)		memcpy(pConfig, &deviceConfig, sizeof(XML_CONFIG_T));

		ret = 0;
	}

	return ret;
}


int	AddElement(char *propertyName, char *propertyValue, TiXmlElement *pParent)
{
	TiXmlElement *pProperty = new TiXmlElement(propertyName);
	TiXmlText* pValue = new TiXmlText(propertyValue);
	pProperty->InsertEndChild(*pValue);
	pParent->InsertEndChild(*pProperty);
	delete pValue;
	delete pProperty;

	return 0;
}

int	AddElement(char *propertyName, int value, TiXmlElement *pParent)
{
	char sztmp[16] = {0};
	sprintf(sztmp, "%d", value);

	TiXmlElement *pProperty = new TiXmlElement(propertyName);
	TiXmlText* pValue = new TiXmlText(sztmp);
	pProperty->InsertEndChild(*pValue);
	pParent->InsertEndChild(*pProperty);
	delete pValue;
	delete pProperty;

	return 0;
}
void	XmlConfig::SaveConfig(const char *filename, XML_CONFIG_T *pConfig)
{
	if (NULL == filename)		return;
	if (NULL != pConfig)
	{
		memcpy(&deviceConfig, pConfig, sizeof(XML_CONFIG_T));
	}

	TiXmlDocument xmlDoc(filename);
	TiXmlDeclaration Declaration( "1.0", "UTF-8", "yes" );
	xmlDoc.InsertEndChild( Declaration );

	TiXmlElement* pRootElm = NULL;
	pRootElm = new TiXmlElement( "XmlConfig" );

	AddElement((char*)"ServerIP", deviceConfig.serverIP, pRootElm);
	AddElement((char*)"ServerPort", deviceConfig.serverPort, pRootElm);
	AddElement((char*)"SSL", deviceConfig.ssl, pRootElm);
	AddElement((char*)"DeviceID", deviceConfig.deviceID, pRootElm);
	AddElement((char*)"SourceURL", deviceConfig.sourceURL, pRootElm);

	xmlDoc.InsertEndChild(*pRootElm) ;

	//xmlDoc.Print() ;
	if (xmlDoc.SaveFile())
	{
	}
	delete pRootElm;
}

