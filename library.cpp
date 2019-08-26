#include "sample_Controller.h"
#include "iostream"
#include "SerialPort.h"

using namespace std;
void Rx_handler(UCHAR rx);

JavaVM *g_jvm = NULL;
jobject g_obj = NULL;

jclass m_class = NULL;
jmethodID m_method = NULL;
jmethodID m_method2 = NULL;
jmethodID m_method3 = NULL;

JNIEXPORT void JNICALL Java_sample_Controller_uartFromc
        (JNIEnv *env, jobject jniCallObj)
{
    env->GetJavaVM(&g_jvm);
    g_obj = env->NewGlobalRef(jniCallObj);

    m_class = env->GetObjectClass(jniCallObj);
    m_method = env->GetStaticMethodID(m_class, "print", "([C)V");
    m_method2 = env->GetStaticMethodID(m_class, "print2", "([C)V");
    m_method3 = env->GetStaticMethodID(m_class, "print3", "(I)V");


    //m_jnienv->CallStaticVoidMethod(m_class, m_method);


    CSerialPort *mySerialPort = new CSerialPort();
    cout << "Hello from C++" << endl;
    int x=0;
    if (!mySerialPort->InitPort(5))
        std::cout << "initPort fail !" << std::endl;
    else
        std::cout << "initPort success !" << std::endl;

    if (!mySerialPort->OpenListenThread())
        std::cout << "OpenListenThread fail !" << std::endl;
    else
        std::cout << "OpenListenThread success !" << std::endl;

    if (!mySerialPort->WriteData((unsigned char*)"12323566\n", 9))
        std::cout << "WriteData fail !" << std::endl;
    else
        std::cout << "WriteData success !" << std::endl;

    env->DeleteGlobalRef(g_obj);
}

void Rx_handler(UCHAR rx)
{
    //cout << rx << endl;
    //x++;
    //cout << x;
    //cout << x << endl;
    printf_s("%02X ", rx);

}

