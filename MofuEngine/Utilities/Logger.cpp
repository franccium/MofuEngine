#include "Logger.h"

namespace mofu::log {
namespace {

ImGuiTextBuffer LogBuffer{};
ImGuiTextFilter Filter{};
ImVector<int> LineOffsets{};
ImVector<LogSeverityLevel> LogLevels{};
bool AutoScroll{ true };

constexpr ImVec4 LOG_LEVEL_COLORS[LogSeverityLevel::Count]{
    {1.f, 1.f, 1.f, 1.f},
    {0.8f, 0.8f, 0.2f, 1.f},
    {1.f, 0.2f, 0.2f, 1.f}
};
constexpr const char* LOG_LEVEL_TAGS[LogSeverityLevel::Count]{ "[inf]: ", "[war]: ", "[err]: " };

void 
AddLog(LogSeverityLevel level, const char* fmt, va_list args) IM_FMTARGS(2)
{
    int old_size = LogBuffer.size();

    LogBuffer.appendf(LOG_LEVEL_TAGS[level]);
    LogBuffer.appendfv(fmt, args);
    LogBuffer.appendf("\n");

    int new_size = LogBuffer.size();
    for(; old_size < new_size; old_size++)
    {
        if (LogBuffer[old_size] == '\n')
        {
            LineOffsets.push_back(old_size + 1);
            LogLevels.push_back(level);
        }
    }
}

}

void 
Initialize()
{
    Clear();
}

void
Clear()
{
    LogBuffer.clear();
    LineOffsets.clear();
    LogLevels.clear();
}


void 
Info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    AddLog(LogSeverityLevel::LogInfo, fmt, args);
    va_end(args);
}

void 
Warn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    AddLog(LogSeverityLevel::LogWarning, fmt, args);
    va_end(args);
}

void 
Error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    AddLog(LogSeverityLevel::LogError, fmt, args);
    va_end(args);
}


void 
Draw(const char* title, bool* p_open)
{
    if (!ImGui::Begin(title, p_open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options"))
    ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0f);

    ImGui::Separator();

    if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = LogBuffer.begin();
        const char* buf_end = LogBuffer.end();
        if (Filter.IsActive())
        {
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                int level{ LogLevels[line_no] };
                if (Filter.PassFilter(line_start, line_end))
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, LOG_LEVEL_COLORS[level]);
                    ImGui::TextUnformatted(line_start, line_end);
                    ImGui::PopStyleColor();
                }
            }
        }
        else
        {
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    int level{ LogLevels[line_no] };
                    ImGui::PushStyleColor(ImGuiCol_Text, LOG_LEVEL_COLORS[level]);
                    ImGui::TextUnformatted(line_start, line_end);
                    ImGui::PopStyleColor();
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

void
AddTestLogs()
{
    static int counter = 0;
    for (int n = 0; n < 5; n++)
    {
        log::Info("this is a test! %d", counter);
        log::Warn("this is a test! %d", counter);
        log::Error("this is a test! %d", counter);
        counter++;
    }
}

}