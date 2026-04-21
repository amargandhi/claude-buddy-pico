// acting.h — shared prompt/result acting grammar for V2 screens.
#pragma once

#include <cstdint>

#include "expression.h"
#include "palette.h"
#include "risk_tier.h"
#include "ui_state.h"

namespace ui_v2::acting {

enum class ApprovalWaitBand : uint8_t {
  None,
  PromptNew,
  PromptWaiting,
  PromptOverdue,
  PromptEscalated,
};

inline bool approval_pending(const BuddyInputs& in) {
  return in.prompt_active && !in.prompt_sent;
}

inline ApprovalWaitBand approval_wait_band(const BuddyInputs& in, uint32_t now_ms) {
  if(!approval_pending(in)) return ApprovalWaitBand::None;
  const uint32_t age_ms = (now_ms >= in.prompt_arrived_ms)
      ? (now_ms - in.prompt_arrived_ms) : 0;
  if(age_ms < 5000u) return ApprovalWaitBand::PromptNew;
  if(age_ms < 15000u) return ApprovalWaitBand::PromptWaiting;
  if(age_ms < 30000u) return ApprovalWaitBand::PromptOverdue;
  return ApprovalWaitBand::PromptEscalated;
}

inline bool prompt_result_active(const BuddyInputs& in, uint32_t now_ms) {
  return in.prompt_result != PromptResult::None &&
         now_ms < in.prompt_result_until_ms;
}

inline uint32_t prompt_result_age_ms(const BuddyInputs& in, uint32_t now_ms) {
  if(now_ms <= in.prompt_result_started_ms) return 0;
  return now_ms - in.prompt_result_started_ms;
}

inline Rgb24 prompt_accent(const BuddyInputs& in) {
  switch(risk::classify(in.prompt_tool)) {
    case RiskTier::Safe:    return kOkGreen;
    case RiskTier::Neutral: return kBusyBlue;
    case RiskTier::Caution: return kWarnAmber;
    case RiskTier::Danger:  return kDangerRed;
  }
  return kDangerRed;
}

inline ExpressionId approval_expression(const BuddyInputs& in, uint32_t now_ms) {
  switch(approval_wait_band(in, now_ms)) {
    case ApprovalWaitBand::PromptNew:       return ExpressionId::Attentive;
    case ApprovalWaitBand::PromptWaiting:   return ExpressionId::Alert;
    case ApprovalWaitBand::PromptOverdue:   return ExpressionId::Concerned;
    case ApprovalWaitBand::PromptEscalated: return ExpressionId::Alert;
    case ApprovalWaitBand::None:            return ExpressionId::Neutral;
  }
  return ExpressionId::Neutral;
}

inline ExpressionId prompt_result_expression(const BuddyInputs& in, uint32_t now_ms) {
  const uint32_t age_ms = prompt_result_age_ms(in, now_ms);
  switch(in.prompt_result) {
    case PromptResult::Approved:
      switch(in.prompt_result_band) {
        case PromptResultBand::Quick:
          return age_ms < 900u ? ExpressionId::Heart : ExpressionId::Proud;
        case PromptResultBand::Normal:
          return age_ms < 900u ? ExpressionId::Relief : ExpressionId::Proud;
        case PromptResultBand::Slow:
          return age_ms < 900u ? ExpressionId::Happy : ExpressionId::Proud;
        case PromptResultBand::None:
          return ExpressionId::Proud;
      }
      break;
    case PromptResult::Denied:
      return age_ms < 120u ? ExpressionId::Squint : ExpressionId::GlyphCaretIn;
    case PromptResult::TimedOut:
      return (in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000u)
          ? ExpressionId::Sleepy : ExpressionId::Sad;
    case PromptResult::None:
      break;
  }
  return ExpressionId::Neutral;
}

inline bool pet_persona_override(const BuddyInputs& in, uint32_t now_ms,
                                 PersonaState& out_persona) {
  if(approval_pending(in)) {
    out_persona = PersonaState::Attention;
    return true;
  }
  if(!prompt_result_active(in, now_ms)) return false;

  switch(in.prompt_result) {
    case PromptResult::Approved:
      out_persona = (in.prompt_result_band == PromptResultBand::Quick)
          ? PersonaState::Heart
          : PersonaState::Celebrate;
      return true;
    case PromptResult::Denied:
      out_persona = PersonaState::Dizzy;
      return true;
    case PromptResult::TimedOut:
    case PromptResult::None:
      return false;
  }
  return false;
}

}  // namespace ui_v2::acting
