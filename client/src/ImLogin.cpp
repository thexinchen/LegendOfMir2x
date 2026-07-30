#include "ImLogin.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>

#include <imgui.h>

#include "audiodevice.hpp"
#include "bgmusicdb.hpp"
#include "buildconfig.hpp"
#include "client.hpp"
#include "clientargparser.hpp"
#include "clientmsg.hpp"
#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_texture.hpp"
#include "idstrf.hpp"
#include "jobf.hpp"
#include "mathf.hpp"
#include "soundeffectdb.hpp"
#include "strf.hpp"
#include "uidf.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;
extern BGMusicDB *g_bgmDB;
extern SoundEffectDB *g_seffDB;
extern PNGTexOffDB *g_selectCharDB;
extern ClientArgParser *g_clientArgParser;

namespace
{
    constexpr ImVec2 screenSize {800.0f, 600.0f};

    void drawTexture(uint32_t textureID, ImVec2 pos = {}, ImVec4 tint = {1, 1, 1, 1})
    {
        if(const auto texture = g_progUseDB->retrieve(textureID); texture){
            ImGui::GetBackgroundDrawList()->AddImage(
                texture,
                pos,
                {pos.x + static_cast<float>(texture.w), pos.y + static_cast<float>(texture.h)},
                {0, 0},
                {1, 1},
                ImGui::ColorConvertFloat4ToU32(tint));
        }
    }

    void drawTextureRegion(uint32_t textureID, ImVec2 pos, ImVec2 size)
    {
        if(const auto texture = g_progUseDB->retrieve(textureID); texture){
            ImGui::GetBackgroundDrawList()->AddImage(
                texture,
                pos,
                {pos.x + size.x, pos.y + size.y},
                {0, 0},
                {size.x / texture.w, size.y / texture.h});
        }
    }

    void drawOffsetTexture(uint32_t textureID, int anchorX, int anchorY, ImU32 tint = IM_COL32_WHITE)
    {
        const auto [texture, dx, dy] = g_selectCharDB->retrieve(textureID);
        if(texture){
            const ImVec2 pos {static_cast<float>(anchorX + dx), static_cast<float>(anchorY + dy)};
            ImGui::GetBackgroundDrawList()->AddImage(
                texture,
                pos,
                {pos.x + texture.w, pos.y + texture.h},
                {0, 0},
                {1, 1},
                tint);
        }
    }

    bool textureButton(const char *id, uint32_t offID, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {static_cast<float>(off.w), static_cast<float>(off.h)});
        const auto state = ImGui::IsItemActive() ? downID : (ImGui::IsItemHovered() ? hoverID : offID);
        const auto texture = g_progUseDB->retrieve(state);
        const auto shown = texture ? texture : off;
        ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        return clicked;
    }

    bool overlayButton(const char *id, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {static_cast<float>(hover.w), static_cast<float>(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto texture = g_progUseDB->retrieve(ImGui::IsItemActive() ? downID : hoverID);
            const auto shown = texture ? texture : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }

    bool beginScreen()
    {
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(screenSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        return ImGui::Begin(
            "##account-flow-screen",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground);
    }

    void endScreen()
    {
        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool transparentInput(const char *id,
                          char *buffer,
                          size_t size,
                          ImVec2 pos,
                          float width,
                          float fontSize,
                          bool password = false,
                          float height = 0.0f)
    {
        float paddingY = 0.0f;
        if(height > 0.0f){
            pos.y -= height * 0.5f;
            const float lineHeight = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "M").y;
            paddingY = std::max(0.0f, (height - lineHeight) * 0.5f);
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::SetNextItemWidth(width);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, paddingY});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
        ImGui::SetWindowFontScale(fontSize / ImGui::GetFontSize());
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if(password){
            flags |= ImGuiInputTextFlags_Password;
        }
        const bool entered = ImGui::InputText(id, buffer, size, flags);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        return entered;
    }

    void drawText(ImVec2 pos, const char *text, float size, ImU32 color = IM_COL32_WHITE)
    {
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), size, pos, color, text);
    }

    void drawRightCenteredText(ImVec2 pos, const char *text, float size, ImU32 color = IM_COL32_WHITE)
    {
        const auto textSize = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        drawText({pos.x - textSize.x, pos.y - textSize.y * 0.5f}, text, size, color);
    }

    void drawCheck(ImVec2 pos, const std::string &value, bool valid)
    {
        if(!value.empty()){
            drawText(pos, valid ? "√" : "×", 15.0f, valid ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
        }
    }

    void drawAccountBackground()
    {
        drawTexture(0X00000003, {0, 75});
        if(const auto texture = g_progUseDB->retrieve(0X00000004); texture){
            ImGui::GetBackgroundDrawList()->AddImage(
                texture,
                {0, 465},
                {800, 465.0f + static_cast<float>(texture.h)});
        }
    }

    void drawFormBackground()
    {
        drawTexture(0X00000003, {0, 75});
        drawTextureRegion(0X00000004, {0, 75}, {800, 450});
    }

    void drawStatusOverlay(const imlogin::Status &status)
    {
        if(!status.active()){
            return;
        }
        auto *drawList = ImGui::GetForegroundDrawList();
        drawList->AddRectFilled({0, 75}, {800, 525}, IM_COL32(0, 0, 255, 32));
        const auto size = ImGui::CalcTextSize(status.text().c_str());
        drawList->AddText(
            {(800.0f - size.x) * 0.5f, 190.0f},
            IM_COL32(255, 255, 0, 255),
            status.text().c_str());
    }

    const char *jobName(uint8_t job)
    {
        switch(jobf::firstJob(job)){
            case JOB_WARRIOR: return "战士";
            case JOB_WIZARD:  return "法师";
            case JOB_TAOIST:  return "道士";
            default:          return "未知";
        }
    }
}

void imlogin::Notice::update(double deltaMS)
{
    for(auto &entry: m_entries){
        entry.remainingMS -= deltaMS;
    }
    while(!m_entries.empty() && m_entries.front().remainingMS <= 0.0){
        m_entries.pop_front();
    }
}

void imlogin::Notice::draw(bool drawBackground) const
{
    if(m_entries.empty()){
        return;
    }
    constexpr float fontSize = 15.0f;
    float maxWidth = 0.0f;
    for(const auto &entry: m_entries){
        maxWidth = std::max(maxWidth, ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, entry.text.c_str()).x);
    }
    const float lineHeight = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "国").y;
    const float height = static_cast<float>(m_entries.size()) * lineHeight;
    const ImVec2 min {(screenSize.x - maxWidth) * 0.5f - 10.0f, (screenSize.y - height) * 0.5f - 10.0f};
    const ImVec2 max {min.x + maxWidth + 20.0f, min.y + height + 20.0f};
    auto *drawList = ImGui::GetForegroundDrawList();
    if(drawBackground){
        drawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 128), 8.0f);
        drawList->AddRect(min, max, IM_COL32(0, 0, 255, 128), 8.0f);
    }
    float y = min.y + 10.0f;
    for(const auto &entry: m_entries){
        const auto width = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, entry.text.c_str()).x;
        drawList->AddText(ImGui::GetFont(), fontSize, {(screenSize.x - width) * 0.5f, y}, IM_COL32(255, 255, 0, 255), entry.text.c_str());
        y += lineHeight;
    }
}

void imlogin::Notice::show(std::string text, double durationMS)
{
    m_entries.push_back({std::move(text), durationMS});
    while(m_entries.size() > m_limit){
        m_entries.pop_front();
    }
}

void imlogin::Notice::clear()
{
    m_entries.clear();
}

void imlogin::Status::update(double deltaMS)
{
    if(!m_persistent && m_remainingMS > 0.0){
        m_remainingMS = std::max(0.0, m_remainingMS - deltaMS);
        if(m_remainingMS == 0.0){
            m_text.clear();
        }
    }
}

void imlogin::Status::show(std::string text, double durationMS)
{
    m_text = std::move(text);
    m_persistent = durationMS <= 0.0;
    m_remainingMS = durationMS;
}

bool imlogin::Status::active() const
{
    return !m_text.empty() && (m_persistent || m_remainingMS > 0.0);
}

const std::string &imlogin::Status::text() const
{
    return m_text;
}

void ProcessLogo::processEvent(const MirEvent &event)
{
    if(event.type == MIR_EVENT_KEY_DOWN && (event.key.key == MIRK_SPACE || event.key.key == MIRK_ESCAPE)){
        g_client->requestProcess(PROCESSID_SYRC);
    }
}

void ProcessLogo::update(double deltaMS)
{
    m_totalTime += deltaMS;
    if(m_totalTime >= 5000.0 || g_clientArgParser->autoLogin){
        g_client->requestProcess(PROCESSID_SYRC);
    }
}

void ProcessLogo::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    const float ratio = static_cast<float>(std::clamp(m_totalTime / 5000.0, 0.0, 1.0));
    const float alpha = ratio < 0.3f ? ratio / 0.3f : (ratio > 0.6f ? (1.0f - ratio) / 0.4f : 1.0f);
    if(const auto texture = g_progUseDB->retrieve(0X00000000); texture){
        ImGui::GetBackgroundDrawList()->AddImage(
            texture,
            {0, 0},
            {static_cast<float>(g_glDevice->getRendererWidth()), static_cast<float>(g_glDevice->getRendererHeight())},
            {0, 0},
            {1, 1},
            ImGui::ColorConvertFloat4ToU32({alpha, alpha, alpha, 1.0f}));
    }
}

void ProcessSync::processEvent(const MirEvent &event)
{
    if(event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        g_client->requestProcess(PROCESSID_LOGIN);
    }
}

void ProcessSync::update(double deltaMS)
{
    if(m_ratio >= 100){
        g_client->requestProcess(PROCESSID_LOGIN);
    }
    else if(deltaMS > 0.0){
        ++m_ratio;
    }
}

void ProcessSync::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawTexture(0X00000001);
    const auto bar = g_progUseDB->retrieve(0X00000002);
    if(bar){
        const float width = bar.w * std::clamp(m_ratio / 100.0f, 0.0f, 1.0f);
        ImGui::GetBackgroundDrawList()->AddImage(bar, {112, 528}, {112 + width, 528.0f + bar.h}, {0, 0}, {width / bar.w, 1});
    }
    if(bar){
        const auto textSize = ImGui::CalcTextSize("Connecting...");
        ImGui::GetForegroundDrawList()->AddText(
            ImGui::GetFont(),
            10.0f,
            {112.0f + bar.w * 0.5f - textSize.x * (10.0f / ImGui::GetFontSize()) * 0.5f, 528.0f + bar.h * 0.5f - 5.0f},
            IM_COL32_WHITE,
            "Connecting...");
    }
}

ProcessLogin::ProcessLogin()
{
    g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040007));
    if(g_clientArgParser->autoLogin){
        sendLogin(g_clientArgParser->autoLogin->first, g_clientArgParser->autoLogin->second);
    }
}

void ProcessLogin::update(double deltaMS) { m_notice.update(deltaMS); }
void ProcessLogin::processEvent(const MirEvent &) {}

void ProcessLogin::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawAccountBackground();
    drawTexture(0X00000011, {103, 536});
    if(beginScreen()){
        if(textureButton("##create-account", 0X00000005, 0X00000006, 0X00000007, {150, 482})){
            g_client->requestProcess(PROCESSID_CREATEACCOUNT);
        }
        if(textureButton("##change-password", 0X00000008, 0X00000009, 0X0000000A, {352, 482})){
            g_client->requestProcess(PROCESSID_CHANGEPASSWORD);
        }
        if(textureButton("##exit", 0X0000000B, 0X0000000C, 0X0000000D, {554, 482})){
            std::exit(0);
        }
        if(textureButton("##login", 0X0000000E, 0X0000000F, 0X00000010, {600, 536})){
            login();
        }
        const bool idEnter = transparentInput("##login-id", m_id.data.data(), m_id.data.size(), {159, 540}, 146, 18);
        const bool passwordEnter = transparentInput("##login-password", m_password.data.data(), m_password.data.size(), {409, 540}, 146, 18, true);
        if(idEnter || passwordEnter){
            login();
        }
        drawText({0, 0}, str_printf("编译版本号:%s", getBuildSignature()).c_str(), 14.0f, IM_COL32(255, 255, 0, 255));
    }
    endScreen();
    m_notice.draw();
}

void ProcessLogin::login() const
{
    if(m_id.str().empty() || m_password.str().empty()){
        m_notice.show("无效的账号或密码");
    }
    else{
        sendLogin(m_id.str(), m_password.str());
    }
}

void ProcessLogin::sendLogin(const std::string &id, const std::string &password) const
{
    CMLogin message {};
    message.id.assign(id);
    message.password.assign(password);
    g_client->send({CM_LOGIN, message});
}

void ProcessLogin::on_SM_LOGINOK(const uint8_t *, size_t) { g_client->requestProcess(PROCESSID_SELECTCHAR); }
void ProcessLogin::on_SM_LOGINERROR(const uint8_t *buf, size_t)
{
    const auto message = ServerMsg::conv<SMLoginError>(buf);
    m_notice.show(message.error == LOGINERR_MULTILOGIN ? "该账号已经登录" : "无效的账号或密码");
}

void ProcessCreateAccount::update(double deltaMS)
{
    m_notice.update(deltaMS);
    m_status.update(deltaMS);
}
void ProcessCreateAccount::processEvent(const MirEvent &) {}

void ProcessCreateAccount::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawFormBackground();
    drawTexture(0X0A000000, {180, 145});
    if(beginScreen()){
        if(!m_status.active()){
            const bool idEnter = transparentInput("##account-id", m_id.data.data(), m_id.data.size(), {315, 230}, 186, 15, false, 28);
            const bool passwordEnter = transparentInput("##account-password", m_password.data.data(), m_password.data.size(), {315, 288}, 186, 15, true, 28);
            const bool confirmEnter = transparentInput("##account-confirm", m_confirm.data.data(), m_confirm.data.size(), {315, 343}, 186, 15, true, 28);
            drawRightCenteredText({299, 230}, "账号", 15);
            drawRightCenteredText({299, 288}, "密码", 15);
            drawRightCenteredText({299, 343}, "确认密码", 15);
            drawCheck({511, 230}, m_id.str(), idstrf::isEmail(m_id.data.data()));
            drawCheck({511, 288}, m_password.str(), idstrf::isPassword(m_password.data.data()));
            drawCheck({511, 343}, m_confirm.str(), idstrf::isPassword(m_confirm.data.data()) && m_password.str() == m_confirm.str());
            if(overlayButton("##account-submit", 0X0800000B, 0X0800000C, {369, 378}) || idEnter || passwordEnter || confirmEnter){
                submit();
            }
        }
        if(overlayButton("##account-return", 0X0000001C, 0X0000001D, {580, 412})){
            g_client->requestProcess(PROCESSID_LOGIN);
        }
    }
    endScreen();
    drawStatusOverlay(m_status);
    m_notice.draw();
}

void ProcessCreateAccount::submit() const
{
    if(!idstrf::isEmail(m_id.data.data())){ m_status.show("无效账号", 2000); clear(); return; }
    if(!idstrf::isPassword(m_password.data.data())){ m_status.show("无效密码", 2000); m_password.clear(); m_confirm.clear(); return; }
    if(m_password.str() != m_confirm.str()){ m_status.show("两次密码输入不一致", 2000); m_password.clear(); m_confirm.clear(); return; }
    CMCreateAccount message {};
    message.id.assign(m_id.str());
    message.password.assign(m_password.str());
    g_client->send({CM_CREATEACCOUNT, message});
    m_status.show("提交中");
}

void ProcessCreateAccount::clear() const { m_id.clear(); m_password.clear(); m_confirm.clear(); }
void ProcessCreateAccount::on_SM_CREATEACCOUNTOK(const uint8_t *, size_t) { m_status.show("注册成功", 2000); }
void ProcessCreateAccount::on_SM_CREATEACCOUNTERROR(const uint8_t *buf, size_t)
{
    const auto error = ServerMsg::conv<SMCreateAccountError>(buf).error;
    clear();
    switch(error){
        case CRTACCERR_ACCOUNTEXIST: m_status.show("账号已存在", 2000); break;
        case CRTACCERR_BADACCOUNT:   m_status.show("无效的账号", 2000); break;
        case CRTACCERR_BADPASSWORD:  m_status.show("无效的密码", 2000); break;
        default: throw fflreach();
    }
}

ProcessSelectChar::ProcessSelectChar()
{
    m_notice.show("正在下载游戏角色");
    g_client->send(CM_QUERYCHAR);
    g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040002));
}

ProcessSelectChar::~ProcessSelectChar()
{
    g_audioDevice->stopBGM();
    g_audioDevice->stopSoundEffect();
}

bool ProcessSelectChar::hasCharacter() const { return m_character && !m_character->name.empty(); }
uint32_t ProcessSelectChar::absoluteFrame() const { return static_cast<uint32_t>(std::lround(m_charAniTime / 200.0)); }

void ProcessSelectChar::update(double deltaMS)
{
    m_notice.update(deltaMS);
    m_charAniTime += deltaMS;
    if(hasCharacter()){
        switchCharacterGfx();
    }
}
void ProcessSelectChar::processEvent(const MirEvent &) {}

void ProcessSelectChar::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawTexture(0X0C000000);
    drawCharacter();
    if(beginScreen()){
        if(hasCharacter()){
            drawCharacterInfo();
            if(!m_openDeletePopup){
                if(textureButton("##select-start", 0X0C000030, 0X0C000030, 0X0C000031, {335, 75})){ start(); }
                if(textureButton("##select-create", 0X0C000010, 0X0C000010, 0X0C000011, {565, 130})){ create(); }
                if(textureButton("##select-delete", 0X0C000020, 0X0C000020, 0X0C000021, {110, 305})){ m_openDeletePopup = true; }
            }
        }
        if(!m_openDeletePopup && textureButton("##select-exit", 0X0C000040, 0X0C000040, 0X0C000041, {45, 544})){
            g_client->requestProcess(PROCESSID_SELECTCHAR);
        }
        if(m_openDeletePopup){
            const auto background = g_progUseDB->retrieve(0X07000000);
            if(background){
                const ImVec2 popupPos {(800.0f - background.w) * 0.5f, (600.0f - background.h) * 0.5f};
                auto *drawList = ImGui::GetWindowDrawList();
                drawList->AddImage(background, popupPos, {popupPos.x + background.w, popupPos.y + background.h});
                drawList->AddText(
                    ImGui::GetFont(),
                    12.0f,
                    {popupPos.x + (background.w - 250.0f) * 0.5f, popupPos.y + 120.0f},
                    IM_COL32_WHITE,
                    "删除的角色将无法还原，请谨慎操作。如果确定删除，请输入游戏密码，并点击YES。",
                    nullptr,
                    250.0f);
                drawList->AddRectFilled({popupPos.x + 22, popupPos.y + 225}, {popupPos.x + 337, popupPos.y + 248}, IM_COL32(255, 255, 255, 32));
                const bool enter = transparentInput(
                    "##delete-password",
                    m_deletePassword.data.data(),
                    m_deletePassword.data.size(),
                    {popupPos.x + 22, popupPos.y + 225},
                    315,
                    14,
                    true);
                if(textureButton("##delete-yes", 0X07000001, 0X07000002, 0X07000003, {popupPos.x + 66, popupPos.y + 190}) || enter){
                    deleteCharacter();
                    m_openDeletePopup = false;
                }
                if(textureButton("##delete-no", 0X07000004, 0X07000005, 0X07000006, {popupPos.x + 212, popupPos.y + 190})){
                    m_deletePassword.clear();
                    m_openDeletePopup = false;
                }
            }
        }
    }
    endScreen();
    m_notice.draw(false);
}

uint32_t ProcessSelectChar::characterFrameCount() const
{
    if(!hasCharacter()){
        return 0;
    }
    const int firstJob = jobf::firstJob(m_character->job);
    const static std::map<std::tuple<int, bool, int>, uint32_t> frameCount
    {
        #include "selectcharframecount.inc"
    };
    if(const auto p = frameCount.find({firstJob, static_cast<bool>(m_character->gender), m_charAni}); p != frameCount.end()){
        return p->second;
    }
    return 0;
}

std::optional<uint32_t> ProcessSelectChar::characterGfxBaseID() const
{
    if(!hasCharacter()){
        return {};
    }
    const auto jobIndex = jobf::jobGfxIndex(m_character->job);
    if(jobIndex.empty()){
        return {};
    }
    return (static_cast<uint32_t>(jobIndex.front()) << 10)
         + (static_cast<uint32_t>(m_character->gender) << 9)
         + (static_cast<uint32_t>(m_charAni) << 5);
}

void ProcessSelectChar::drawCharacter() const
{
    const auto frameCount = characterFrameCount();
    const auto baseID = characterGfxBaseID();
    if(frameCount == 0 || !baseID){
        return;
    }
    constexpr uint32_t shadowMask = UINT32_C(1) << 14;
    constexpr uint32_t magicMask = UINT32_C(1) << 13;
    const uint32_t frameID = *baseID + absoluteFrame() % frameCount;
    drawOffsetTexture(frameID, 430, 300);
    drawOffsetTexture(frameID | shadowMask, 430, 300, IM_COL32(255, 255, 255, 150));
    drawOffsetTexture(frameID, 430, 300);
    drawOffsetTexture(frameID | magicMask, 430, 300);
}

void ProcessSelectChar::drawCharacterInfo() const
{
    const auto name = m_character->name.to_str();
    const std::string character = str_printf("角色：%s", name.c_str());
    const std::string level = str_printf("等级：%d", static_cast<int>(SYS_LEVEL(m_character->exp)));
    const std::string profession = str_printf("职业：%s", jobName(m_character->job));
    constexpr float fontSize = 15.0f;
    const float lineHeight = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "国").y;
    const float boardWidth = std::max({
        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, character.c_str()).x,
        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, level.c_str()).x,
        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, profession.c_str()).x,
    });
    const float boardHeight = lineHeight * 3.0f;
    const float boardY = 260.0f - boardHeight;
    auto *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled({105, boardY - 15}, {120 + boardWidth + 15, 275}, IM_COL32(0, 0, 0, 128), 5);
    drawList->AddRect({105, boardY - 15}, {120 + boardWidth + 15, 275}, IM_COL32(231, 231, 189, 128), 5);
    drawText({120, boardY}, character.c_str(), fontSize, IM_COL32(237, 226, 200, 255));
    drawText({120, boardY + lineHeight}, level.c_str(), fontSize, IM_COL32(175, 196, 175, 255));
    drawText({120, boardY + lineHeight * 2.0f}, profession.c_str(), fontSize, IM_COL32(231, 231, 189, 255));
}

void ProcessSelectChar::switchCharacterGfx()
{
    const auto frameCount = characterFrameCount();
    if(frameCount == 0 || absoluteFrame() % frameCount || m_charAniSwitchFrame == absoluteFrame()){
        return;
    }
    m_charAniSwitchFrame = absoluteFrame();
    switch(m_charAni){
        case 0: m_charAni = mathf::rand<int>(0, 1) == 0 ? 1 : 2; break;
        case 1: m_charAni = 2; break;
        case 2: m_charAni = mathf::rand<int>(0, 1) == 0 ? 2 : 3; break;
        case 3: m_charAni = 0; break;
        default: m_charAni = 0; break;
    }
}

void ProcessSelectChar::start() const
{
    if(hasCharacter()){ g_client->send(CM_ONLINE); }
    else{ m_notice.show("请先创建游戏角色"); }
}

void ProcessSelectChar::create() const
{
    if(hasCharacter()){ m_notice.show("一个账号只能创建一个游戏角色"); }
    else if(m_character){ g_client->requestProcess(PROCESSID_CREATECHAR); }
    else{ m_notice.show("正在下载游戏角色"); }
}

void ProcessSelectChar::deleteCharacter() const
{
    if(!hasCharacter()){ m_notice.show("此账号没有角色"); return; }
    if(m_deletePassword.str().empty()){ m_notice.show("无效的密码"); return; }
    CMDeleteChar message {};
    message.password.assign(m_deletePassword.str());
    g_client->send({CM_DELETECHAR, message});
    m_deletePassword.clear();
}

void ProcessSelectChar::on_SM_QUERYCHAROK(const uint8_t *buf, size_t)
{
    m_character = ServerMsg::conv<SMQueryCharOK>(buf);
    m_notice.clear();
    if(g_clientArgParser->autoLogin){ start(); }
}

void ProcessSelectChar::on_SM_QUERYCHARERROR(const uint8_t *buf, size_t)
{
    const auto error = ServerMsg::conv<SMQueryCharError>(buf).error;
    if(error != QUERYCHARERR_NOCHAR){ throw fflvalue(error); }
    m_character.emplace();
    m_character->name.clear();
    m_notice.show("请先创建游戏角色");
}

void ProcessSelectChar::on_SM_DELETECHAROK(const uint8_t *, size_t)
{
    m_character.emplace();
    m_character->name.clear();
    m_notice.show("删除角色成功");
}

void ProcessSelectChar::on_SM_DELETECHARERROR(const uint8_t *buf, size_t)
{
    switch(ServerMsg::conv<SMDeleteCharError>(buf).error){
        case DELCHARERR_BADPASSWORD: m_notice.show("密码错误"); break;
        case DELCHARERR_NOCHAR:      m_notice.show("没有角色可以删除"); break;
        case DELCHARERR_DBERROR:     m_notice.show("删除角色失败，请稍后重试"); break;
        default: throw fflreach();
    }
}

void ProcessSelectChar::on_SM_ONLINEOK(const uint8_t *buf, size_t)
{
    const auto message = ServerMsg::conv<SMOnlineOK>(buf);
    fflassert(uidf::isPlayer(message.uid), message.uid);
    fflassert(DBCOM_MAPRECORD(uidf::getMapID(message.mapUID)), message.mapUID);
    g_client->setOnlineOK(message);
    g_client->requestProcess(PROCESSID_RUN);
}

void ProcessSelectChar::on_SM_ONLINEERROR(const uint8_t *buf, size_t)
{
    switch(ServerMsg::conv<SMOnlineError>(buf).error){
        case ONLINEERR_NOCHAR:      m_notice.show("先创建角色以运行游戏"); break;
        case ONLINEERR_MULTIONLINE: m_notice.show("请勿频繁登录"); break;
        default: throw fflreach();
    }
}

ProcessCreateChar::ProcessCreateChar() { g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040001)); }
ProcessCreateChar::~ProcessCreateChar() { g_audioDevice->stopBGM(); g_audioDevice->stopSoundEffect(); }
uint32_t ProcessCreateChar::absoluteFrame() const { return static_cast<uint32_t>(std::lround(m_animationTime / 200.0)); }

void ProcessCreateChar::update(double deltaMS)
{
    m_notice.update(deltaMS);
    m_animationTime += deltaMS;
    if(const auto frameCount = characterFrameCount(m_job, m_male); frameCount > 0){
        if(const auto frame = absoluteFrame(); frame % frameCount == 0 && m_lastStartFrame != frame){
            playMagicSoundEffect();
            m_lastStartFrame = frame;
        }
    }
}
void ProcessCreateChar::processEvent(const MirEvent &) {}

void ProcessCreateChar::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawTexture(0X0D000000);
    drawCharacter(true, 193, 215);
    drawCharacter(false, 495, 220);
    drawTexture(0X0D000002, {268, 555}, {1, 1, 1, 150.0f / 255.0f});
    drawTexture(0X0D000001, {320, 500});
    if(beginScreen()){
        ImGui::GetWindowDrawList()->AddRectFilled({355, 520}, {445, 535}, IM_COL32_BLACK);
        ImGui::GetWindowDrawList()->AddRect({355, 520}, {445, 535}, IM_COL32(231, 231, 189, 100));
        const bool enter = transparentInput("##character-name", m_name.data.data(), m_name.data.size(), {355, 520}, 85, 12);

        ImGui::SetCursorScreenPos({200, 290});
        if(ImGui::InvisibleButton("##male-character", {90, 260})){
            m_male = true;
            m_animationTime = 0.0;
            m_lastStartFrame = std::numeric_limits<uint32_t>::max();
        }
        ImGui::SetCursorScreenPos({510, 310});
        if(ImGui::InvisibleButton("##female-character", {90, 260})){
            m_male = false;
            m_animationTime = 0.0;
            m_lastStartFrame = std::numeric_limits<uint32_t>::max();
        }

        if(textureButton("##warrior", 0X0D000030, 0X0D000031, 0X0D000032, {339, 539})){ m_job = JOB_WARRIOR; }
        if(textureButton("##wizard", 0X0D000040, 0X0D000041, 0X0D000042, {381, 539})){ m_job = JOB_WIZARD; }
        if(textureButton("##taoist", 0X0D000050, 0X0D000051, 0X0D000052, {424, 539})){ m_job = JOB_TAOIST; }
        if(textureButton("##create-submit", 0X0D000010, 0X0D000011, 0X0D000012, {512, 549}) || enter){ submit(); }
        if(textureButton("##create-exit", 0X0D000020, 0X0D000021, 0X0D000022, {554, 549})){
            g_client->requestProcess(PROCESSID_SELECTCHAR);
        }
    }
    endScreen();
    m_notice.draw();
}

void ProcessCreateChar::submit() const
{
    if(m_name.str().empty() || m_name.str().size() >= SYS_NAMESIZE){ m_notice.show("无效的角色名"); return; }
    CMCreateChar message {};
    message.name.assign(m_name.str());
    message.job = static_cast<uint8_t>(m_job);
    message.gender = static_cast<uint8_t>(m_male);
    g_client->send({CM_CREATECHAR, message});
    m_notice.show("提交中");
}

uint32_t ProcessCreateChar::characterFrameCount(int job, bool gender)
{
    const static std::map<std::tuple<int, bool, int>, uint32_t> frameCount
    {
        #include "selectcharframecount.inc"
    };
    if(const auto p = frameCount.find({job, gender, 4}); p != frameCount.end()){
        return p->second;
    }
    return 0;
}

uint32_t ProcessCreateChar::characterGfxBaseID(int job, bool gender)
{
    return (static_cast<uint32_t>(job - JOB_BEGIN) << 10)
         + (static_cast<uint32_t>(gender) << 9)
         + (UINT32_C(4) << 5);
}

void ProcessCreateChar::drawCharacter(bool gender, int drawX, int drawY) const
{
    const auto frameCount = characterFrameCount(m_job, gender);
    if(frameCount == 0){
        return;
    }
    constexpr uint32_t shadowMask = UINT32_C(1) << 14;
    constexpr uint32_t magicMask = UINT32_C(1) << 13;
    const bool active = gender == m_male;
    const auto frameID = characterGfxBaseID(m_job, gender) + (active ? absoluteFrame() % frameCount : 0);
    if(!gender && m_job == JOB_WARRIOR){
        drawY += 40;
    }
    const ImU32 fullTint = active ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255);
    const ImU32 alphaTint = active ? IM_COL32(255, 255, 255, 150) : IM_COL32(128, 128, 128, 150);
    drawOffsetTexture(frameID, drawX, drawY, fullTint);
    drawOffsetTexture(frameID | shadowMask, drawX, drawY, alphaTint);
    drawOffsetTexture(frameID, drawX, drawY, fullTint);
    drawOffsetTexture(frameID | magicMask, drawX, drawY, fullTint);
}

void ProcessCreateChar::playMagicSoundEffect() const
{
    const uint32_t soundID = UINT32_C(0X00010000)
        | (static_cast<uint32_t>(m_male) << 4)
        | (static_cast<uint32_t>(m_job - JOB_BEGIN) << 8);
    g_audioDevice->playSoundEffect(g_seffDB->retrieve(soundID));
}

void ProcessCreateChar::on_SM_CREATECHAROK(const uint8_t *, size_t) { g_client->requestProcess(PROCESSID_SELECTCHAR); }
void ProcessCreateChar::on_SM_CREATECHARERROR(const uint8_t *buf, size_t)
{
    switch(ServerMsg::conv<SMCreateCharError>(buf).error){
        case CRTCHARERR_BADNAME:    m_notice.show("无效的角色名"); break;
        case CRTCHARERR_BADGENDER:  m_notice.show("无效的角色性别"); break;
        case CRTCHARERR_BADJOB:     m_notice.show("无效的角色职业"); break;
        case CRTCHARERR_CHAREXIST:  m_notice.show("一个账号只能创建一个角色"); break;
        case CRTCHARERR_NAMEEXIST:  m_notice.show("角色名已被使用"); break;
        default: throw fflreach();
    }
}

void ProcessChangePassword::update(double deltaMS)
{
    m_notice.update(deltaMS);
    m_status.update(deltaMS);
}
void ProcessChangePassword::processEvent(const MirEvent &) {}

void ProcessChangePassword::draw() const
{
    GLDeviceHelper::RenderNewFrame frame;
    drawFormBackground();
    drawTexture(0X0A000001, {180, 145});
    if(beginScreen()){
        if(!m_status.active()){
            const bool idEnter = transparentInput("##change-id", m_id.data.data(), m_id.data.size(), {315, 224}, 186, 15, false, 28);
            const bool oldEnter = transparentInput("##change-old", m_password.data.data(), m_password.data.size(), {315, 271}, 186, 15, true, 28);
            const bool newEnter = transparentInput("##change-new", m_newPassword.data.data(), m_newPassword.data.size(), {315, 318}, 186, 15, true, 28);
            const bool confirmEnter = transparentInput("##change-confirm", m_confirm.data.data(), m_confirm.data.size(), {315, 365}, 186, 15, true, 28);
            drawRightCenteredText({299, 224}, "账号", 15);
            drawRightCenteredText({299, 271}, "密码", 15);
            drawRightCenteredText({299, 318}, "新密码", 15);
            drawRightCenteredText({299, 365}, "确认密码", 15);
            drawCheck({511, 224}, m_id.str(), idstrf::isEmail(m_id.data.data()));
            drawCheck({511, 271}, m_password.str(), idstrf::isPassword(m_password.data.data()));
            drawCheck({511, 318}, m_newPassword.str(), idstrf::isPassword(m_newPassword.data.data()));
            drawCheck({511, 365}, m_confirm.str(), idstrf::isPassword(m_confirm.data.data()) && m_newPassword.str() == m_confirm.str());
            if(overlayButton("##change-submit", 0X0800000B, 0X0800000C, {369, 399}) || idEnter || oldEnter || newEnter || confirmEnter){
                submit();
            }
        }
        if(overlayButton("##change-return", 0X0000001C, 0X0000001D, {580, 433})){
            g_client->requestProcess(PROCESSID_LOGIN);
        }
    }
    endScreen();
    drawStatusOverlay(m_status);
    m_notice.draw();
}

void ProcessChangePassword::submit() const
{
    if(!idstrf::isEmail(m_id.data.data())){ m_status.show("无效账号", 2000); clear(); return; }
    if(!idstrf::isPassword(m_password.data.data())){ m_status.show("无效密码", 2000); m_password.clear(); m_newPassword.clear(); m_confirm.clear(); return; }
    if(!idstrf::isPassword(m_newPassword.data.data())){ m_status.show("无效新密码", 2000); m_newPassword.clear(); m_confirm.clear(); return; }
    if(m_newPassword.str() != m_confirm.str()){ m_status.show("新密码两次输入不一致", 2000); m_newPassword.clear(); m_confirm.clear(); return; }
    if(m_password.str() == m_newPassword.str()){ m_status.show("新旧密码相同", 2000); m_newPassword.clear(); m_confirm.clear(); return; }
    CMChangePassword message {};
    message.id.assign(m_id.str());
    message.password.assign(m_password.str());
    message.passwordNew.assign(m_newPassword.str());
    g_client->send({CM_CHANGEPASSWORD, message});
    m_status.show("提交中");
}

void ProcessChangePassword::clear() const
{
    m_id.clear(); m_password.clear(); m_newPassword.clear(); m_confirm.clear();
}

void ProcessChangePassword::on_SM_CHANGEPASSWORDOK(const uint8_t *, size_t) { m_status.show("修改密码成功", 2000); }
void ProcessChangePassword::on_SM_CHANGEPASSWORDERROR(const uint8_t *buf, size_t)
{
    const auto error = ServerMsg::conv<SMChangePasswordError>(buf).error;
    clear();
    switch(error){
        case CHGPWDERR_BADACCOUNT:         m_status.show("无效的账号", 2000); break;
        case CHGPWDERR_BADPASSWORD:        m_status.show("无效的密码", 2000); break;
        case CHGPWDERR_BADNEWPASSWORD:     m_status.show("无效的新密码", 2000); break;
        case CHGPWDERR_BADACCOUNTPASSWORD: m_status.show("错误的账号或密码", 2000); break;
        default: throw fflreach();
    }
}
