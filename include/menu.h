#include <string>
#include <iostream>
#include <TFT_eSPI.h>
#include <vector>
#include <list>
using namespace std;

struct MenuInfo
{
    MenuInfo()
    {
        nIndex = 0;
        pParent = NULL;
        pCallBack = NULL;
    }
    MenuInfo(string strName)
    {
        MenuInfo();
        strMenuName = strName;
    }
    void SetCallBackFun(std::function<void()> pFun){
        pCallBack = pFun;
    }
    string strMenuName;
    int nIndex;
    vector<MenuInfo*> Chiles;
    MenuInfo *pParent;
    std::function<void()> pCallBack;
};

class CMyMenu
{
private:
    /* data */
public:
    CMyMenu(TFT_eSPI *pTft);
    ~CMyMenu();

    void DrawMainMenu();
    void ShowCurMenu();
    void OnUpClick();
    void onDownClick();
    void onEnterClick();
    void SetNeedRefresh();
    bool OnMenuClick();

private:
    void IniMenu();

private:
    uint8_t m_currentSelection = 0;
    TFT_eSPI *m_pTft;
    MenuInfo *m_pCurMenu;
    MenuInfo m_Menu;
    bool m_bNeedRefresh;
};
