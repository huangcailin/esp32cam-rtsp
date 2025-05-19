#include "menu.h"
#include <esp_log.h>
const char *MTAG = "MY_Menu";

CMyMenu::CMyMenu(TFT_eSPI *pTft) : m_bNeedRefresh(true), m_pCurMenu(NULL)
{
    m_pTft = pTft;
    IniMenu();
}

CMyMenu::~CMyMenu()
{
}

void CMyMenu::IniMenu()
{
    MenuInfo *pSysConfMenu = new MenuInfo("System");
    MenuInfo *pCamConfMenu = new MenuInfo("Camera");
    MenuInfo *pAboutMenu = new MenuInfo("About");
    pSysConfMenu->pParent = &m_Menu;
    pCamConfMenu->pParent = &m_Menu;
    pAboutMenu->pParent = &m_Menu;
    m_Menu.Chiles.push_back(pSysConfMenu);
    m_Menu.Chiles.push_back(pCamConfMenu);
    m_Menu.Chiles.push_back(pAboutMenu);

    MenuInfo *pAboutInfo = new MenuInfo("empty");
    pAboutInfo->pParent = pAboutMenu;
    pAboutMenu->Chiles.push_back(pAboutInfo);
}

void CMyMenu::DrawMainMenu()
{
    if (m_pCurMenu == &m_Menu && !m_bNeedRefresh)
        return;
    m_pCurMenu = &m_Menu;
    if (!m_pTft)
        return;
    m_bNeedRefresh = false;
    m_pTft->fillScreen(TFT_BLACK);
    m_pTft->setTextColor(TFT_WHITE, TFT_BLACK);
    m_pTft->setTextDatum(TC_DATUM); // 顶部居中
    m_pTft->setTextSize(2);
    ShowCurMenu();
}

void CMyMenu::ShowCurMenu()
{
    for (uint8_t i = 0; i < m_pCurMenu->Chiles.size(); i++)
    {
        if (i == m_currentSelection)
        {
            m_pTft->fillRoundRect(20, 20 + i * 30, 200, 20, 5, TFT_BLUE);
            m_pTft->setTextColor(TFT_WHITE);
        }
        else
        {
            m_pTft->setTextColor(TFT_WHITE);
        }
        m_pTft->drawString(m_pCurMenu->Chiles[i]->strMenuName.c_str(), m_pTft->width() / 2, 22 + i * 30, 1);
    }
}

void CMyMenu::OnUpClick()
{
    if (m_currentSelection > 0)
        m_currentSelection--;
    SetNeedRefresh();
    ShowCurMenu();
}

void CMyMenu::onDownClick()
{
    m_currentSelection++;
    if (m_currentSelection >= m_pCurMenu->Chiles.size())
        m_currentSelection = 0;
    SetNeedRefresh();
    ShowCurMenu();
}

void CMyMenu::onEnterClick()
{
    size_t iCount = m_pCurMenu->Chiles.size();
    if (0 > m_currentSelection || m_currentSelection > iCount - 1)
        return;
    iCount = m_pCurMenu->Chiles[m_currentSelection]->Chiles.size();
    if (iCount == 0)
    {
        if (m_pCurMenu->pCallBack)
            m_pCurMenu->pCallBack();
        return;
    }
    m_pCurMenu = m_pCurMenu->Chiles[m_currentSelection];
    m_currentSelection = 0;
    SetNeedRefresh();
    ShowCurMenu();
}

void CMyMenu::SetNeedRefresh()
{
    m_pTft->fillScreen(TFT_BLACK); // 清屏
    m_bNeedRefresh = true;
}

bool CMyMenu::OnMenuClick()
{
    if (m_pCurMenu->pParent)
    {
        m_pCurMenu = m_pCurMenu->pParent;
        ShowCurMenu();
        return true;
    }
    else
        return false;
}
