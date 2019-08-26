// SerialPort.cpp : 定义控制台应用程序的入口点。
//

//////////////////////////////////////////////////////////////////////////  
/// COPYRIGHT NOTICE  
/// Copyright (c) 2009, 华中科技大学tickTick Group  （版权声明）  
/// All rights reserved.  
///   
/// @file    SerialPort.cpp    
/// @brief   串口通信类的实现文件  
///  
/// 本文件为串口通信类的实现代码  
///  
/// @version 1.0     
/// @author  卢俊    
/// @E-mail：lujun.hust@gmail.com  
/// @date    2010/03/19  
///   
///  
///  修订说明：  
//////////////////////////////////////////////////////////////////////////  

#include <process.h>    
#include <iostream>    
#include "SerialPort.h"  
#include "jni.h"

using namespace std;
/** 线程退出标志 */
bool CSerialPort::s_bExit = false;
/** 当串口无数据时,sleep至下次查询间隔的时间,单位:秒 */
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

//初始化串口函数
bool CSerialPort::InitPort(UINT portNo /*= 1*/, UINT baud /*= CBR_9600*/, char parity /*= 'N'*/,
	UINT databits /*= 8*/, UINT stopsbits /*= 1*/, DWORD dwCommEvents /*= EV_RXCHAR*/)
{

	/** 临时变量,将制定参数转化为字符串形式,以构造DCB结构 */
	char szDCBparam[50];
	sprintf_s(szDCBparam, "baud=%d parity=%c data=%d stop=%d", baud, parity, databits, stopsbits);

	/** 打开指定串口,该函数内部已经有临界区保护,上面请不要加保护 */
	if (!openPort(portNo))
	{
		return false;

	}

	/** 进入临界段 */
	EnterCriticalSection(&m_csCommunicationSync);

	/** 是否有错误发生 */
	BOOL bIsSuccess = TRUE;

	/** 在此可以设置输入输出的缓冲区大小,如果不设置,则系统会设置默认值.
	*  自己设置缓冲区大小时,要注意设置稍大一些,避免缓冲区溢出
	*/
	/*if (bIsSuccess )
	{
	bIsSuccess = SetupComm(m_hComm,10,10);
	}*/

	/** 设置串口的超时时间,均设为0,表示不使用超时限制 */
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
		//// 将ANSI字符串转换为UNICODE字符串  
		//DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, NULL, 0);
		//wchar_t *pwText = new wchar_t[dwNum];
		//if (!MultiByteToWideChar(CP_ACP, 0, szDCBparam, -1, pwText, dwNum))
		//{
		//	bIsSuccess = TRUE;
		//}

		/** 获取当前串口配置参数,并且构造串口DCB参数 */
		bIsSuccess = GetCommState(m_hComm, &dcb) && BuildCommDCB(_T("baud=9600 parity=N data=8 stop=1"), &dcb);//((LPCTSTR)szDCBparam, &dcb);

		cout << bIsSuccess<<endl;//"这儿啊！";
		/** 开启RTS flow控制 */
		dcb.fRtsControl = RTS_CONTROL_ENABLE;

		/** 释放内存空间 */
		/*delete[] pwText;*/
	}

	if (bIsSuccess)
	{
		/** 使用DCB参数配置串口状态 */
		bIsSuccess = SetCommState(m_hComm, &dcb);
	}

	/**  清空串口缓冲区 */
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	/** 离开临界段 */
	LeaveCriticalSection(&m_csCommunicationSync);

	return bIsSuccess == TRUE;
}

//初始化串口函数
bool CSerialPort::InitPort(UINT portNo, const LPDCB& plDCB)
{
	/** 打开指定串口,该函数内部已经有临界区保护,上面请不要加保护 */
	if (!openPort(portNo))
	{

		return false;
	}

	/** 进入临界段 */
	EnterCriticalSection(&m_csCommunicationSync);

	/** 配置串口参数 */
	if (!SetCommState(m_hComm, plDCB))
	{
		return false;
	}

	/**  清空串口缓冲区 */
	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	/** 离开临界段 */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}

//关闭关闭串口
void CSerialPort::ClosePort()
{
	/** 如果有串口被打开，关闭它 */
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}

//打开出串口
bool CSerialPort::openPort(UINT portNo)
{
	/** 进入临界段 */
	EnterCriticalSection(&m_csCommunicationSync);

	/** 把串口的编号转换为设备名 */
	char szPort[50];
	sprintf_s(szPort, "COM%d", portNo);

	/** 打开指定的串口 */
	m_hComm = CreateFileA(szPort,  /** 设备名,COM1,COM2等 */
		GENERIC_READ | GENERIC_WRITE, /** 访问模式,可同时读写 */
		0,                            /** 共享模式,0表示不共享 */
		NULL,                         /** 安全性设置,一般使用NULL */
		OPEN_EXISTING,                /** 该参数表示设备必须存在,否则创建失败 */
		0,
		0);

	/** 如果打开失败，释放资源并返回 */
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	/** 退出临界区 */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}

//打开监听线程
bool CSerialPort::OpenListenThread()
{
	/** 检测线程是否已经开启了 */
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		/** 线程已经开启 */
		return false;
	}
	s_bExit = false;
	/** 线程ID */
	UINT threadId;
	/** 开启串口数据监听线程 */

	m_hListenThread = (HANDLE)_beginthreadex(NULL, 0, ListenThread, this, 0, &threadId);
	if (!m_hListenThread)
	{
		return false;
	}
	/** 设置线程的优先级,高于普通线程 */
	if (!SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL))
	{
		return false;
	}

	return true;
}
//关闭监听线程
bool CSerialPort::CloseListenThread()
{
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		/** 通知线程退出 */
		s_bExit = true;

		/** 等待线程退出 */
		Sleep(10);

		/** 置线程句柄无效 */
		CloseHandle(m_hListenThread);
		m_hListenThread = INVALID_HANDLE_VALUE;
	}
	return true;
}
//获取串口缓冲区的字节数
UINT CSerialPort::GetBytesInCOM()
{
	DWORD dwError = 0;  /** 错误码 */
	COMSTAT  comstat;   /** COMSTAT结构体,记录通信设备的状态信息 */
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT BytesInQue = 0;
	/** 在调用ReadFile和WriteFile之前,通过本函数清除以前遗留的错误标志 */
	if (ClearCommError(m_hComm, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue; /** 获取在输入缓冲区中的字节数 */
	}

	return BytesInQue;
}
//串口监听线程
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


	/** 得到本类的指针 */
	CSerialPort *pSerialPort = reinterpret_cast<CSerialPort*>(pParam);

	// 线程循环,轮询方式读取串口数据  
	while (!pSerialPort->s_bExit)
	{
		UINT BytesInQue = pSerialPort->GetBytesInCOM();
		/** 如果串口输入缓冲区中无数据,则休息一会再查询 */
		if (BytesInQue == 0)
		{
			Sleep(SLEEP_TIME_INTERVAL);
			continue;
		}

		/** 读取输入缓冲区中的数据并输出显示 */
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

//------------------------------热图
				realen++;

				if (flag == 1)
				{
					ch[index] = cRecved;
					index++;
				}
				if (((old & 0xff) << 8 | (cRecved & 0xff)) == 0xffd6 && flag == 0)
				{
					index = 0;//热图
					flag = 1;
					memset(ch, '\0', sizeof(ch));//热图
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
					memset(ch, '\0', sizeof(ch));//热图
					index = 0;//热图
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
//读取串口接收缓冲区中一个字节的数据
bool CSerialPort::ReadChar(unsigned char &cRecved)
{
	BOOL  bResult = TRUE;
	DWORD BytesRead = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	/** 临界区保护 */
	EnterCriticalSection(&m_csCommunicationSync);

	/** 从缓冲区读取一个字节的数据 */
	bResult = ReadFile(m_hComm, &cRecved, 1, &BytesRead, NULL);
	if ((!bResult))
	{
		/** 获取错误码,可以根据该错误码查出错误原因 */
		DWORD dwError = GetLastError();

		/** 清空串口缓冲区 */
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);

		return false;
	}

	/** 离开临界区 */
	LeaveCriticalSection(&m_csCommunicationSync);

	return (BytesRead == 1);

}

// 向串口写数据, 将缓冲区中的数据写入到串口
bool CSerialPort::WriteData(unsigned char *pData, int length)
{
	int *pData1 = new int;
	BOOL   bResult = TRUE;
	DWORD  BytesToSend = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	/** 临界区保护 */
	EnterCriticalSection(&m_csCommunicationSync);

	/** 向缓冲区写入指定量的数据 */
	bResult = WriteFile(m_hComm,/*文件句柄*/pData,/*用于保存读入数据的一个缓冲区*/ 8,/*要读入的字符数*/ &BytesToSend,/*指向实际读取字节数的指针*/ NULL);
	if (!bResult)
	{
		DWORD dwError = GetLastError();
		/** 清空串口缓冲区 */
		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);

		return false;
	}

	/** 离开临界区 */
	LeaveCriticalSection(&m_csCommunicationSync);

	return true;
}
