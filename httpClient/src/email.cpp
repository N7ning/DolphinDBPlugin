#include <Exceptions.h>
#include <ScalarImp.h>
#include <openssl/ssl.h>
#include <Util.h>
#include "email.h"
#include <curl/curl.h>
#include<list>
#include<vector>
#include<string>

size_t curlWriteData(void *ptr, size_t size, size_t nmemb, string *data) {
    data->append((char *)ptr, size * nmemb);
    return size * nmemb;
}

CSendMail::CSendMail( 
                const std::string & strUser,
                const std::string & strPsw
            )
{
    m_strUser = strUser;
    m_strPsw = strPsw;
    m_RecipientList.clear();
    m_MailContent.clear();
    m_iMailContentPos = 0;
}

size_t CSendMail::read_callback(void* ptr, size_t size, size_t nmemb, void* userp)
{
    CSendMail * pSm = (CSendMail *)userp;

    if(size*nmemb < 1)
        return 0;
    if(pSm->m_iMailContentPos < (int)pSm->m_MailContent.size())
    {
        size_t len;
        len = pSm->m_MailContent[pSm->m_iMailContentPos].length();

        memcpy(ptr, pSm->m_MailContent[pSm->m_iMailContentPos].c_str(), pSm->m_MailContent[pSm->m_iMailContentPos].length());
        pSm->m_iMailContentPos++; 
        return len;
    }
    return 0;
}

bool CSendMail::ConstructHead(const std::string & strSubject, const std::string & strContent)
{
    m_MailContent.push_back("MIME-Versioin: 1.0\n");
    std::string strTemp = "To: ";
    for(std::list<std::string >::iterator it = m_RecipientList.begin(); it != m_RecipientList.end();)
    {
        strTemp += *it;
        it++;
        if(it != m_RecipientList.end())
                strTemp += ",";
    }  
    strTemp += "\n";
    m_MailContent.push_back(strTemp);
    if(strSubject != "")
    {
        strTemp = "Subject: ";
        strTemp += strSubject;
        strTemp += "\n";
        m_MailContent.push_back(strTemp);
    }
    m_MailContent.push_back("Content-Transfer-Encoding: 8bit\n");
    m_MailContent.push_back("Content-Type: text/html; \n Charset=\"UTF-8\"\n\n");
    if(strContent != "")
    {
        m_MailContent.push_back(strContent);
    }
   
    return true;
}

ConstantSP CSendMail::SendMail(const std::string& strSubject, const std::string& strMailBody)
{
    ConstantSP res = Util::createDictionary(DT_STRING, nullptr, DT_ANY, nullptr);
    char errorBuf[CURL_ERROR_SIZE];
    m_MailContent.clear();
    m_iMailContentPos = 0;
    ConstructHead(strSubject, strMailBody);
    CURL *curl;
    struct curl_slist* rcpt_list = NULL;
    SSL_library_init();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (!curl)
    {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        throw IllegalArgumentException(__FUNCTION__, "Init curl failed");
        return res;
    }
    std::string strListMailTo;
    for(std::list<std::string >::iterator it = m_RecipientList.begin(); it != m_RecipientList.end();it++)
    {
        strListMailTo+=it->c_str();
        strListMailTo+=' ';
    }
    for(std::list<std::string >::iterator it = m_RecipientList.begin(); it != m_RecipientList.end();it++)
    {
        rcpt_list = curl_slist_append(rcpt_list, it->c_str());
    } 
    string strSmtpServer;
    int port=25;
    int pos=m_strUser.find_first_of('@');
    string tmp=m_strUser.substr(pos+1);
    if(tmp=="126.com")
    {
        strSmtpServer="smtp.126.com";
        port=25;
    }
    else if(tmp=="163.com")
    {
        strSmtpServer="smtp.163.com";
        port=25;
    }
    else if(tmp=="yeah.net")
    {
        strSmtpServer="smtp.yeah.net";
        port=25;
    }
    else if(tmp=="sohu.com")
    {
        strSmtpServer="smtp.sohu.com";
        port=25;
    }
    else if(tmp=="dolphindb.com")
    {
        strSmtpServer="smtp.ionos.com";
        port=25;
    }
    else 
        throw IllegalArgumentException(__FUNCTION__, "Enter @126.com,@163.com,@yeah.net,@sohu.com,@dolphindb.com");
    
    std::string strUrl = "smtp://" + strSmtpServer;
    strUrl += ":";
    strUrl += std::to_string(port);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuf);
    curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME,  m_strUser.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD,  m_strPsw.c_str());
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &read_callback);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM,m_strUser.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt_list);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long) CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_READDATA, this);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);    
    string responseString;
    string headerString;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerString);
    CURLcode ret = curl_easy_perform(curl);
    if (ret != 0) {
        string errorMsg(errorBuf);
        curl_slist_free_all(rcpt_list);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        throw RuntimeException("failed to send the email,send it again, or change the subject and body of the email to avoid misreading it as a robot, or change the email address.");
        return res;
    }

    long responseCode;
    double elapsed;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
    if(responseCode!=250)
    {
        curl_slist_free_all(rcpt_list);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        throw RuntimeException("failed to send the email,send it again, or change the subject and body of the email to avoid misreading it as a robot, or change the email address.");
    }
    res->set(new String("responseCode"), new Int(responseCode));
    res->set(new String("elapsed"), new Double(elapsed));
    res->set(new String("headers"), new String(headerString));
    res->set(new String("text"), new String(responseString));
    res->set(new String("userId"), new String(m_strUser));
    res->set(new String("recipient"), new String(strListMailTo));
    curl_slist_free_all(rcpt_list);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return res;
}

ConstantSP sendEmail(Heap *heap, vector<ConstantSP> &args)
{
    ConstantSP userId,pwd;
    ConstantSP recipient,subject,body;
    userId=args[0];
    if (userId->getType() != DT_STRING || userId->getForm() != DF_SCALAR)
        throw IllegalArgumentException(__FUNCTION__, "UserId must be a string scalar");
    pwd=args[1];
    if (pwd->getType() != DT_STRING || pwd->getForm() != DF_SCALAR)
        throw IllegalArgumentException(__FUNCTION__, "Pwd must be a string scalar");
    recipient=args[2];
    if ((recipient->getType() != DT_STRING || recipient->getForm() != DF_SCALAR)&&
    (recipient->getType() != DT_STRING || recipient->getForm() != DF_VECTOR))
        throw IllegalArgumentException(__FUNCTION__, "recipient must be a string scalar or a string vector");
    subject=args[3];
    if (subject->getType() != DT_STRING || subject->getForm() != DF_SCALAR)
        throw IllegalArgumentException(__FUNCTION__, "Suject must be a string scalar");
    body=args[4];
    if (body->getType() != DT_STRING || body->getForm() != DF_SCALAR)
        throw IllegalArgumentException(__FUNCTION__, "Body must be a string scalar");
    string strinUser=userId->getString(),strinPsw=pwd->getString();
    string strinSubject=subject->getString(),strinContext=body->getString();
    CSendMail mail(strinUser,strinPsw);
    if(recipient->getForm()==DF_VECTOR)
    {
        Vector* veMailTo=((VectorSP)recipient).get();
        for(int i=0;i<veMailTo->size();++i)
        {
            mail.AddRecipient(veMailTo->get(i)->getString());
        }
    }
    else 
        mail.AddRecipient(recipient->getString());
    ConstantSP res=mail.SendMail(strinSubject,strinContext);
    return res;
}
