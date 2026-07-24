#include "pngtexdb.hpp"
#include "processrun.hpp"
#include "acutionboard.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

AcutionBoard::AcutionBoard(ProcessRun *argProc, Widget *argParent, bool argAutoDelete)
    : Widget
      {{
          .dir = DIR_NONE,

          .x = [](const Widget *){ return g_glDevice->getRendererWidth () / 2; },
          .y = [](const Widget *){ return g_glDevice->getRendererHeight() / 2; },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_runProc(argProc)
    , m_background
      {{
          .texLoadFunc = [](const Widget *) -> GLTexID 
          {
              return g_progUseDB->retrieve(0X00001400);
          },

          .blendMode = MIR_BLENDMODE_NONE,
          .parent{this},
      }}
{}
