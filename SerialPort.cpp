// SerialPort.cpp : �������̨Ӧ�ó������ڵ㡣
//

//////////////////////////////////////////////////////////////////////////  
/// COPYRIGHT NOTICE  
/// Copyright (c) 2009, ���пƼ���ѧtickTick Group  ����Ȩ������  
/// All rights reserved.  
///   
/// @file    SerialPort.cpp    
/// @brief   ����ͨ�����ʵ���ļ�  
///  
/// ���ļ�Ϊ����ͨ�����ʵ�ִ���  
///  
/// @version 1.0     
/// @author  ¬��    
/// @E-mail��lujun.hust@gmail.com  
/// @date    2010/03/19  
///   
///  
///  �޶�˵����  
//////////////////////////////////////////////////////////////////////////  

#include <process.h>    
#include <iostream>    
#include "SerialPort.h"  
#include "jni.h"

using namespace std;
/** �߳��˳���־ */
bool CSerialPort::s_bExit = false;
/** ������������ʱ,sleep���´β�ѯ�����ʱ��,��λ:�� */
const UINT SLEEP_TIME_INTERVAL = 5;

extern void Rx_handler(UCHAR rx);
extern JavaVM *g_jvm;
extern jobject g_obj;
extern jmethodID m_method;
extern jmethodID m_method2;
extern jmethodID m_method3;

CSerialPort::CSerialPort(void)
	: m_hListenThread(INVALID_HANDLE_VALUE)
{
	m_hComm = INVALID_HANDLE_VALUE;
	m_hListenThread = INVALID_HANDLE_VALUE;
	InitializeCriticalSection(&m_csCommunicationSync);
}

CSerialPort::~CSerialPort(void)
{
	CloseListenThread();
	ClosePort();
	DeleteCriticalSection(&m_csCommunicationSync);
}

//��ʼ�����ں���
bool CSerialPort::InitPort(UINT portNo /*= 1*/, UINT baud /*= CBR_9600*/, char parity /*= 'N'*/,
	UINT databits /*= 8*/, UINT stopsbits /*= 1*/, DWORD dwCommEvents /*= EV_RXCHAR*/)
{

	/** ��ʱ����,���ƶ�����ת��Ϊ�ַ�����ʽ,�Թ���DCB�ṹ */
	char szDCBparam[50];
	sprintf_s(szDCBparam, "baud=%d parity=%c data=%d stop=%d", baud, parity, databits, stopsbits);

	/** ��ָ������,�ú����ڲ��Ѿ����ٽ�������,�����벻Ҫ�ӱ��� */
	if (!openPort(portNo))
	{
		return false;

	}

	/** �����ٽ�� */
	EnterCriticalSection(&m_csCommunicationSync);

	/** �Ƿ��д����� */
	BOOL bIsSuccess = TRUE;

	/** �ڴ˿���������������Ļ�������С,���������,��ϵͳ������Ĭ��ֵ.
	*  �Լ����û�������Сʱ,Ҫע�������Դ�һЩ,���⻺�������
	*/
	/*if (bIsSuccess )
	{
	bIsSuccess = SetupComm(m_hComm,10,10);
	}*/

	/** ���ô��ڵĳ�ʱʱ��,����Ϊ0,��ʾ��ʹ�ó�ʱ���� */
	COMMTIMEOUTS  CommTimeouts;
	CommTimeouts.ReadIntervalTimeout = 0;
	CommTimeouts.ReadTotalTimeoutMultiplier = 0;
	CommTimeouts.ReadTotalTimeoutConstant = 0;
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;
	CommTimeouts.WriteTotalTimeoutConstant = 0;
	if (bIsSuccess)
	{
		bIsSuccess = SetCommTimeouts(m_hComm, &CommTimeouts);
	}

	DCB  dcb;
	if (bIsSuccess)
	{
		//// ��ANSI�ַ���ת��ΪUNICODE�ַ���  
		//DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, NULL, 0);
		//wchar_t *pwText = new wchar_t[dwNum];
		//if (!MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, pwText, dwNum))
		//{
		//	bIsSuccess = TRUE;
		//}

		/** ��ȡ��ǰ�������ò���,���ҹ��촮��DCB���� */
		bIsSuccess = GetCommState(m_hComm, &dcb) && BuildCommDCB(_T("baud=9600 parity=N data=8 stop=1"), &dcb);//((LPCTSTR)szDCBparam, &dcb);

		cout << bIsSuccess<<endl;//"�������";
		/** ����RTS flow���� */
		dcb.fRtsControl = RTS_CONTROL_ENABLE;

		/** �ͷ��ڴ�ռ� */
		/*delete[] pwText;*/
	}

	if (bIsSuccess)
	{
		/** ʹ��DCB�������ô���״̬ */
		bIsSuccess = SetCommState(m_hComm, &dcb);
	}

	/**  ��մ��ڻ����� */
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	/** �뿪�ٽ�� */
	LeaveCriticalSection(&m_csCommunicationSync);

	return bIsSuccess == TRUE;
}

//��ʼ�����ں���
bool CSerialPort::InitPort(UINT portNo, const LPDCB& plDCB)
{
	/** ��ָ������,�ú����ڲ��Ѿ����ٽ�������,�����벻Ҫ�ӱ��� */
	if (!openPort(portNo))
	{

		return false;
	}

	/** �����ٽ�� */
	EnterCriticalSection(&m_csCommunicationSync);

	/** ���ô��ڲ��� */
	if (!SetCommState(m_hComm, plDCB))
	{
		return false;
	}

	/**  ��մ��ڻ����� */
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	/** �뿪�ٽ�� */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}

//�رչرմ���
void CSerialPort::ClosePort()
{
	/** ����д��ڱ��򿪣��ر��� */
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}

//�򿪳�����
bool CSerialPort::openPort(UINT portNo)
{
	/** �����ٽ�� */
	EnterCriticalSection(&m_csCommunicationSync);

	/** �Ѵ��ڵı��ת��Ϊ�豸�� */
	char szPort[50];
	sprintf_s(szPort, "COM%d", portNo);

	/** ��ָ���Ĵ��� */
	m_hComm = CreateFileA(szPort,  /** �豸��,COM1,COM2�� */
		GENERIC_READ | GENERIC_WRITE, /** ����ģʽ,��ͬʱ��д */
		0,                            /** ����ģʽ,0��ʾ������ */
		NULL,                         /** ��ȫ������,һ��ʹ��NULL */
		OPEN_EXISTING,                /** �ò�����ʾ�豸�������,���򴴽�ʧ�� */
		0,
		0);

	/** �����ʧ�ܣ��ͷ���Դ������ */
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	/** �˳��ٽ��� */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}

//�򿪼����߳�
bool CSerialPort::OpenListenThread()
{
	/** ����߳��Ƿ��Ѿ������� */
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		/** �߳��Ѿ����� */
		return false;
	}
	s_bExit = false;
	/** �߳�ID */
	UINT threadId;
	/** �����������ݼ����߳� */

	m_hListenThread = (HANDLE)_beginthreadex(NULL, 0, ListenThread, this, 0, &threadId);
	if (!m_hListenThread)
	{
		return false;
	}
	/** �����̵߳����ȼ�,������ͨ�߳� */
	if (!SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL))
	{
		return false;
	}

	return true;
}
//�رռ����߳�
bool CSerialPort::CloseListenThread()
{
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		/** ֪ͨ�߳��˳� */
		s_bExit = true;

		/** �ȴ��߳��˳� */
		Sleep(10);

		/** ���߳̾����Ч */
		CloseHandle(m_hListenThread);
		m_hListenThread = INVALID_HANDLE_VALUE;
	}
	return true;
}
//��ȡ���ڻ��������ֽ���
UINT CSerialPort::GetBytesInCOM()
{
	DWORD dwError = 0;  /** ������ */
	COMSTAT  comstat;   /** COMSTAT�ṹ��,��¼ͨ���豸��״̬��Ϣ */
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT BytesInQue = 0;
	/** �ڵ���ReadFile��WriteFile֮ǰ,ͨ�������������ǰ�����Ĵ����־ */
	if (ClearCommError(m_hComm, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue; /** ��ȡ�����뻺�����е��ֽ��� */
	}

	return BytesInQue;
}
//���ڼ����߳�
UINT WINAPI CSerialPort::ListenThread(void* pParam)
{
	JNIEnv *localEnv;
	jclass cls;

	if (g_jvm->AttachCurrentThread((void **)&localEnv, NULL) != JNI_OK) {
		cout << "error1" << endl;
		return NULL;
	}

	cls = localEnv->GetObjectClass(g_obj);
	if (cls == NULL) {
		cout << "error2"<<endl;
	}

	jchar arr[204800];
	char ch[204800] = { static_cast<char>(0xFF),static_cast<char>(0xD8) };
	int len = 2,index=2,realen=0;
	char old=0,flag=0;

	len = strlen(ch);
	for (int x = 0; x < len; x++)
	{
		arr[x] = ch[x];
	}
	//printf_s("%X",(ch[0]&0xff)<<8| (ch[1] & 0xff));
	if (((ch[0] & 0xff) << 8 | (ch[1] & 0xff)) == 0xffd8)
	{
		jcharArray array = localEnv->NewCharArray(len);
		jchar char2 = 0xff;
		localEnv->SetCharArrayRegion(array, 0, len, arr);
		localEnv->CallStaticVoidMethod(cls, m_method, array);
		localEnv->CallStaticVoidMethod(cls, m_method2, ch[2]);
		localEnv->CallStaticVoidMethod(cls, m_method3, 3333);
	}


	/** �õ������ָ�� */
	CSerialPort *pSerialPort = reinterpret_cast<CSerialPort*>(pParam);

	// �߳�ѭ��,��ѯ��ʽ��ȡ��������  
	while (!pSerialPort->s_bExit)
	{
		UINT BytesInQue = pSerialPort->GetBytesInCOM();
		/** ����������뻺������������,����Ϣһ���ٲ�ѯ */
		if (BytesInQue == 0)
		{
			Sleep(SLEEP_TIME_INTERVAL);
			continue;
		}

		/** ��ȡ���뻺�����е����ݲ������ʾ */
		unsigned char cRecved = 0x00;


		do
		{
			cRecved = 0x00;
			if (pSerialPort->ReadChar(cRecved) == true)
			{

				//realen++;
				//if (flag == 1)
				//{
				//	ch[index] = cRecved;
				//	index++;
				//}
				//if (((old & 0xff) << 8 | (cRecved & 0xff)) == 0xffd8 && flag == 0)
				//{
				//	index = 2;
				//	flag = 1;
				//	memset(ch+2, '\0', sizeof(ch));
				//}else
				//if (((old & 0xff) << 8 | (cRecved & 0xff)) == 0xffd9 && flag == 1)
				//{
				//	len = index;
				//	for (int x = 0; x < len; x++)
				//	{
				//		arr[x] = ch[x];
				//	}
				//	jcharArray array = localEnv->NewCharArray(len);
				//	localEnv->SetCharArrayRegion(array, 0, len, arr);
				//	localEnv->CallStaticVoidMethod(cls, m_method, array);
				//	memset(ch+2, '\0', sizeof(ch));
				//	index = 2;
				//	flag = 0;

				//	//localEnv->CallStaticVoidMethod(cls, m_method2, realen);
				//}

//------------------------------��ͼ
				realen++;

				if (flag == 1)
				{
					ch[index] = cRecved;
					index++;
				}
				if (((old & 0xff) << 8 | (cRecved & 0xff)) == 0xffd6 && flag == 0)
				{
					index = 0;//��ͼ
					flag = 1;
					memset(ch, '\0', sizeof(ch));//��ͼ
				}else
				if (((old & 0xff) << 8 | (cRecved & 0xff)) == 0xffd7 && flag == 1)
				{
					len = index-2;
					for (int x = 0; x < len; x++)
					{
						arr[x] = ch[x];
					}
					jcharArray array = localEnv->NewCharArray(len);
					localEnv->SetCharArrayRegion(array, 0, len, arr);
					localEnv->CallStaticVoidMethod(cls, m_method2, array);
					memset(ch, '\0', sizeof(ch));//��ͼ
					index = 0;//��ͼ
					flag = 0;

					localEnv->CallStaticVoidMethod(cls, m_method3, realen);
				}



				old = cRecved;
				continue;
			}
		} while (--BytesInQue);
	}

	return 0;
}
//��ȡ���ڽ��ջ�������һ���ֽڵ�����
bool CSerialPort::ReadChar(unsigned char &cRecved)
{
	BOOL  bResult = TRUE;
	DWORD BytesRead = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	/** �ٽ������� */
	EnterCriticalSection(&m_csCommunicationSync);

	/** �ӻ�������ȡһ���ֽڵ����� */
	bResult = ReadFile(m_hComm, &cRecved, 1, &BytesRead, NULL);
	if ((!bResult))
	{
		/** ��ȡ������,���Ը��ݸô�����������ԭ�� */
		DWORD dwError = GetLastError();

		/** ��մ��ڻ����� */
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);

		return false;
	}

	/** �뿪�ٽ��� */
	LeaveCriticalSection(&m_csCommunicationSync);

	return (BytesRead == 1);

}

// �򴮿�д����, ���������е�����д�뵽����
bool CSerialPort::WriteData(unsigned char *pData, int length)
{
	int *pData1 = new int;
	BOOL   bResult = TRUE;
	DWORD  BytesToSend = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	/** �ٽ������� */
	EnterCriticalSection(&m_csCommunicationSync);

	/** �򻺳���д��ָ���������� */
	bResult = WriteFile(m_hComm,/*�ļ����*/pData,/*���ڱ���������ݵ�һ��������*/ 8,/*Ҫ������ַ���*/ &BytesToSend,/*ָ��ʵ�ʶ�ȡ�ֽ�����ָ��*/ NULL);
	if (!bResult)
	{
		DWORD dwError = GetLastError();
		/** ��մ��ڻ����� */
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);

		return false;
	}

	/** �뿪�ٽ��� */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}
