#include <sstream>

#include <imgui.h>
#include <sol/sol.hpp>

#include "log.hpp"
#include "luaf.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "fflerror.hpp"
#include "threadpool.hpp"
#include "actorpool.hpp"
#include "server.hpp"
#include "commandluamodule.hpp"
#include "guicore.hpp"
#include "guicommandwindow.hpp"

extern Server *g_server;
extern ActorPool *g_actorPool;

GUICommandWindow::~GUICommandWindow() = default;
GUICommandWindow::Slot::~Slot() = default;

GUICommandWindow::Slot *GUICommandWindow::getSlot(int cwid)
{
    if(cwid >= 1 && cwid <= 16){
        return m_slotList.at(cwid).get();
    }
    return nullptr;
}

std::vector<std::string> GUICommandWindow::getHistory(int cwid)
{
    if(auto *slot = getSlot(cwid)){
        std::lock_guard<std::mutex> lockGuard(slot->historyLock);
        return slot->history;
    }
    return {};
}

int GUICommandWindow::createCommandWindow()
{
    for(int cwid = 1; cwid <= 16; ++cwid){
        if(m_slotList.at(cwid)){
            continue;
        }

        auto slot = std::make_unique<Slot>();
        slot->cwid      = to_u32(cwid);
        slot->luaModule = std::make_unique<CommandLuaModule>(to_u32(cwid));
        slot->worker    = std::make_unique<threadPool>(1);

        g_server->regLuaExport(slot->luaModule.get(), to_u32(cwid));

        m_slotList.at(cwid) = std::move(slot);
        return cwid;
    }
    return 0;
}

void GUICommandWindow::deleteCommandWindow(int cwid)
{
    // threadPool dtor joins the worker, so a running eval finishes first
    if(getSlot(cwid)){
        m_slotList.at(cwid).reset();
    }
}

void GUICommandWindow::appendCWLog(uint32_t cwid, int type, const char *prompt, const char *text)
{
    if(auto *slot = getSlot(to_d(cwid))){
        slot->logList.push_back({type, prompt ? prompt : "", text ? text : ""});
        if(slot->logList.size() > 2000){
            slot->logList.erase(slot->logList.begin(), slot->logList.begin() + 500);
        }
    }
}

void GUICommandWindow::execString(int cwid, const std::string &code)
{
    auto *slot = getSlot(cwid);
    if(!(slot && str_haschar(code.c_str()))){
        return;
    }

    if(slot->busy){
        g_server->addCWLogString(to_u32(cwid), 2, ">>> ", str_printf("Command window %d is busy", cwid).c_str());
        return;
    }

    slot->busy = true;
    slot->justFinished = false;

    slot->worker->addTask([this, cwid, code](int)
    {
        auto *taskSlot = getSlot(cwid);
        if(!taskSlot){
            return;
        }

        const auto fnEvalLuaStr = [taskSlot](const char *luaCode)
        {
            switch(taskSlot->evalMode){
                case 0: // AUTO
                    if(g_actorPool->running()){
                        return taskSlot->luaModule->execString("asyncEval(%s)", luaf::quotedLuaString(luaCode).c_str());
                    }
                    return taskSlot->luaModule->execRawString(luaCode);
                case 1: // LOCAL
                    return taskSlot->luaModule->execRawString(luaCode);
                case 2: // ASYNC
                    if(g_actorPool->running()){
                        return taskSlot->luaModule->execString("asyncEval(%s)", luaf::quotedLuaString(luaCode).c_str());
                    }
                    throw fflpanic("actor pool not running");
                default:
                    throw fflpanic("invalid eval mode: {}", taskSlot->evalMode);
            }
        };

        try{
            if(const auto callResult = fnEvalLuaStr(code.c_str()); callResult.valid()){
                // success prints nothing by default
            }
            else{
                sol::error err = callResult;
                std::stringstream errStream(err.what());

                std::string errLine;
                while(std::getline(errStream, errLine, '\n')){
                    g_server->addCWLogString(to_u32(cwid), 2, ">>> ", errLine.c_str());
                }
            }
        }
        catch(const std::exception &e){
            g_server->addCWLogString(to_u32(cwid), 2, ">>> ", e.what());
        }
        catch(...){
            g_server->addCWLogString(to_u32(cwid), 2, ">>> ", "unknown error");
        }

        taskSlot->busy = false;
        taskSlot->justFinished = true;
    });
}

static int fnHistoryCallback(ImGuiInputTextCallbackData *data)
{
    auto *slot = static_cast<GUICommandWindow::Slot *>(data->UserData);
    if(slot->history.empty()){
        return 0;
    }

    // history cursor: -1 = current input; navigate with Up/Down
    static thread_local int s_cursor = -1; // per-widget navigation is single-threaded on the GUI frame
    switch(data->EventKey){
        case ImGuiKey_UpArrow:
            {
                if(data->CursorPos != 0 || data->SelectionStart != data->SelectionEnd){
                    break;
                }
                if(s_cursor < 0){
                    s_cursor = to_d(slot->history.size()) - 1;
                }
                else if(s_cursor > 0){
                    s_cursor--;
                }
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, slot->history.at(s_cursor).c_str());
                break;
            }
        case ImGuiKey_DownArrow:
            {
                if(data->CursorPos != 0 || data->SelectionStart != data->SelectionEnd){
                    break;
                }
                if(s_cursor >= 0){
                    s_cursor++;
                    if(s_cursor >= to_d(slot->history.size())){
                        s_cursor = -1;
                    }
                }
                data->DeleteChars(0, data->BufTextLen);
                if(s_cursor >= 0){
                    data->InsertChars(0, slot->history.at(s_cursor).c_str());
                }
                break;
            }
        default:
            break;
    }
    return 0;
}

void GUICommandWindow::drawSlot(int cwid, Slot &slot)
{
    bool open = true;
    const auto title = str_printf("Command Window %d###CW%d", cwid, cwid);

    ImGui::SetNextWindowSize(ImVec2(580, 400), ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(title.c_str(), &open)){
        ImGui::End();
        if(!open){
            deleteCommandWindow(cwid);
        }
        return;
    }

    ImGui::RadioButton("AUTO", &slot.evalMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("LOCAL", &slot.evalMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("ASYNC", &slot.evalMode, 2);
    ImGui::Separator();

    // log pane
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if(ImGui::BeginChild("CWLog", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders)){
        for(const auto &entry: slot.logList){
            ImGui::PushStyleColor(ImGuiCol_Text, entry.type == 2 ? IM_COL32(240, 90, 90, 255) : IM_COL32(210, 210, 210, 255));
            ImGui::TextUnformatted((entry.prompt + entry.text).c_str());
            ImGui::PopStyleColor();
        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f){
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    // input line (Enter executes, Up/Down browse history; Shift+Enter multiline
    // from the FLTK version is not reproduced -- single-line console input)
    if(slot.justFinished){
        slot.justFinished = false;
        ImGui::SetKeyboardFocusHere();
    }

    if(slot.busy){
        ImGui::BeginDisabled();
    }

    bool exec = ImGui::InputText("##cwinput", slot.inputBuf, sizeof(slot.inputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            fnHistoryCallback, &slot);

    if(slot.busy){
        ImGui::EndDisabled();
    }

    if(exec && !slot.busy && str_haschar(slot.inputBuf)){
        const std::string code(slot.inputBuf);
        slot.inputBuf[0] = '\0';
        {
            std::lock_guard<std::mutex> lockGuard(slot.historyLock);
            slot.history.push_back(code);
        }
        g_server->addCWLogString(to_u32(cwid), 0, "<<< ", code.c_str()); // echo like the legacy input
        execString(cwid, code);
    }

    ImGui::End();
    if(!open){
        deleteCommandWindow(cwid);
    }
}

void GUICommandWindow::drawAllWindows()
{
    for(int cwid = 1; cwid <= 16; ++cwid){
        if(auto &slotPtr = m_slotList.at(cwid); slotPtr && slotPtr->open){
            drawSlot(cwid, *slotPtr);
        }
    }
}
