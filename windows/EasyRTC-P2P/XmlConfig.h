#pragma once


#define	XML_CONFIG_FILENAME "EasyRTC-P2P.xml"


typedef struct __XML_CONFIG_T
{
	char		serverIP[32];
	int			serverPort;
	int			ssl;
	char		deviceID[64];
	char		sourceURL[1024];
}XML_CONFIG_T;

class XmlConfig
{
public:
	XmlConfig(void);
	~XmlConfig(void);

	int		LoadConfig(const char *filename, XML_CONFIG_T *pConfig);
	void	SaveConfig(const char *filename, XML_CONFIG_T *pConfig);
	//int		LoadConfigToJson(int serverType, char *filename, char *outJsonStr);

	XML_CONFIG_T	*GetConfig()	{return &deviceConfig;}
protected:

	XML_CONFIG_T		deviceConfig;
};

