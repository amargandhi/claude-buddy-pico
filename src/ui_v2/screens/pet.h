// pet.h — pet / mood screen (plan §6.10, revised).
//
// Amar's feedback: pet stays in the MAIN cycle, not hidden in a submenu.
//
// The pet screen is a more playful face-dominant layout: eyes switch
// between Heart / Joy / Dizzy based on recent interactions, with a mood
// indicator below the face and level/nap stats on the side. It's the
// "personality" surface, as opposed to the face-as-status on Home.
//
// Buttons:
//   A Menu | B Feed (cycles expression) | X Next (Info) | Y Back (Home)
#pragma once
#include "screen.h"
namespace ui_v2::screens::pet { const ScreenVTable* vtable(); }
