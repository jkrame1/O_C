// Copyright (c) 2015, 2016 Patrick Dowling, Tim Churches
//
// Initial app implementation: Patrick Dowling (pld@gurkenkiste.com)
// Modifications by: Tim Churches (tim.churches@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Trigger-driven Neo-Riemannian Tonnetz transformations to generate chords

#include "OC_bitmaps.h"
#include "OC_strings.h"
#include "tonnetz/tonnetz_state.h"
#include "util/util_settings.h"
#include "util/util_ringbuffer.h"

// NOTE: H1200 state is updated in the ISR, and we're accessing shared state
// (e.g. outputs) without any sync mechanism. So there is a chance of the
// display being slightly inconsistent but the risk seems acceptable.
// Similarly for changing settings, but this should also be safe(ish) since
// - Each setting should be atomic (int)
// - Changing more than one settings happens seldomly
// - Settings aren't modified in the ISR

enum OutputMode {
  OUTPUT_CHORD_VOICING,
  OUTPUT_TUNE,
  OUTPUT_MODE_LAST
};

enum TransformPriority {
  TRANSFORM_PRIO_XPLR,
  TRANSFORM_PRIO_XLRP,
  TRANSFORM_PRIO_XRPL,
  TRANSFORM_PRIO_XPRL,
  TRANSFORM_PRIO_XRLP,
  TRANSFORM_PRIO_XLPR,
  TRANSFORM_PRIO_LAST
};

enum H1200Setting {
  H1200_SETTING_ROOT_OFFSET,
  H1200_SETTING_OCTAVE,
  H1200_SETTING_MODE,
  H1200_SETTING_INVERSION,
  H1200_SETTING_TRANFORM_PRIO,
  H1200_SETTING_OUTPUT_MODE,
  H1200_SETTING_TRIGGER_TYPE,
  H1200_SETTING_ROOT_EUCLIDEAN_LENGTH,
  H1200_SETTING_ROOT_EUCLIDEAN_FILL,
  H1200_SETTING_ROOT_EUCLIDEAN_OFFSET,
  H1200_SETTING_P_EUCLIDEAN_LENGTH,
  H1200_SETTING_P_EUCLIDEAN_FILL,
  H1200_SETTING_P_EUCLIDEAN_OFFSET,
  H1200_SETTING_L_EUCLIDEAN_LENGTH,
  H1200_SETTING_L_EUCLIDEAN_FILL,
  H1200_SETTING_L_EUCLIDEAN_OFFSET,
  H1200_SETTING_R_EUCLIDEAN_LENGTH,
  H1200_SETTING_R_EUCLIDEAN_FILL,
  H1200_SETTING_R_EUCLIDEAN_OFFSET,
  H1200_SETTING_N_EUCLIDEAN_LENGTH,
  H1200_SETTING_N_EUCLIDEAN_FILL,
  H1200_SETTING_N_EUCLIDEAN_OFFSET,
  H1200_SETTING_S_EUCLIDEAN_LENGTH,
  H1200_SETTING_S_EUCLIDEAN_FILL,
  H1200_SETTING_S_EUCLIDEAN_OFFSET,
  H1200_SETTING_H_EUCLIDEAN_LENGTH,
  H1200_SETTING_H_EUCLIDEAN_FILL,
  H1200_SETTING_H_EUCLIDEAN_OFFSET,
  H1200_SETTING_LAST
};

enum H1200TriggerTypes {
  H1200_TRIGGER_TYPE_PLR,
  H1200_TRIGGER_TYPE_EUCLIDEAN,
  H1200_TRIGGER_TYPE_LAST
};


class H1200Settings : public settings::SettingsBase<H1200Settings, H1200_SETTING_LAST> {
public:

  int root_offset() const {
    return values_[H1200_SETTING_ROOT_OFFSET];
  }

  int octave() const {
    return values_[H1200_SETTING_OCTAVE];
  }

  EMode mode() const {
    return static_cast<EMode>(values_[H1200_SETTING_MODE]);
  }

  int inversion() const {
    return values_[H1200_SETTING_INVERSION];
  }

  TransformPriority get_transform_priority() const {
    return static_cast<TransformPriority>(values_[H1200_SETTING_TRANFORM_PRIO]);
  }

  OutputMode output_mode() const {
    return static_cast<OutputMode>(values_[H1200_SETTING_OUTPUT_MODE]);
  }

  H1200TriggerTypes get_trigger_type() const {
    return static_cast<H1200TriggerTypes>(values_[H1200_SETTING_TRIGGER_TYPE]);
  }

  uint8_t get_root_euclidean_length() const {
    return values_[H1200_SETTING_ROOT_EUCLIDEAN_LENGTH];
  }

  uint8_t get_root_euclidean_fill() const {
    return values_[H1200_SETTING_ROOT_EUCLIDEAN_FILL];
  }

  uint8_t get_root_euclidean_offset() const {
    return values_[H1200_SETTING_ROOT_EUCLIDEAN_OFFSET];
  }

  uint8_t get_p_euclidean_length() const {
    return values_[H1200_SETTING_P_EUCLIDEAN_LENGTH];
  }

  uint8_t get_p_euclidean_fill() const {
    return values_[H1200_SETTING_P_EUCLIDEAN_FILL];
  }

  uint8_t get_p_euclidean_offset() const {
    return values_[H1200_SETTING_P_EUCLIDEAN_OFFSET];
  }

  uint8_t get_l_euclidean_length() const {
    return values_[H1200_SETTING_L_EUCLIDEAN_LENGTH];
  }

  uint8_t get_l_euclidean_fill() const {
    return values_[H1200_SETTING_L_EUCLIDEAN_FILL];
  }

  uint8_t get_l_euclidean_offset() const {
    return values_[H1200_SETTING_L_EUCLIDEAN_OFFSET];
  }

  uint8_t get_r_euclidean_length() const {
    return values_[H1200_SETTING_R_EUCLIDEAN_LENGTH];
  }

  uint8_t get_r_euclidean_fill() const {
    return values_[H1200_SETTING_R_EUCLIDEAN_FILL];
  }

  uint8_t get_r_euclidean_offset() const {
    return values_[H1200_SETTING_R_EUCLIDEAN_OFFSET];
  }

  uint8_t get_n_euclidean_length() const {
    return values_[H1200_SETTING_N_EUCLIDEAN_LENGTH];
  }

  uint8_t get_n_euclidean_fill() const {
    return values_[H1200_SETTING_N_EUCLIDEAN_FILL];
  }

  uint8_t get_n_euclidean_offset() const {
    return values_[H1200_SETTING_N_EUCLIDEAN_OFFSET];
  }

  uint8_t get_s_euclidean_length() const {
    return values_[H1200_SETTING_S_EUCLIDEAN_LENGTH];
  }

  uint8_t get_s_euclidean_fill() const {
    return values_[H1200_SETTING_S_EUCLIDEAN_FILL];
  }

  uint8_t get_s_euclidean_offset() const {
    return values_[H1200_SETTING_S_EUCLIDEAN_OFFSET];
  }

  uint8_t get_h_euclidean_length() const {
    return values_[H1200_SETTING_H_EUCLIDEAN_LENGTH];
  }

  uint8_t get_h_euclidean_fill() const {
    return values_[H1200_SETTING_H_EUCLIDEAN_FILL];
  }

  uint8_t get_h_euclidean_offset() const {
    return values_[H1200_SETTING_H_EUCLIDEAN_OFFSET];
  }

  void Init() {
    InitDefaults();
    update_enabled_settings();
  }

  H1200Setting enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  void update_enabled_settings() {
    
    H1200Setting *settings = enabled_settings_;

    *settings++ =   H1200_SETTING_ROOT_OFFSET;
    *settings++ =   H1200_SETTING_OCTAVE;
    *settings++ =   H1200_SETTING_MODE;
    *settings++ =   H1200_SETTING_INVERSION;
    *settings++ =   H1200_SETTING_TRANFORM_PRIO;
    *settings++ =   H1200_SETTING_OUTPUT_MODE;
    *settings++ =   H1200_SETTING_TRIGGER_TYPE;
    
 
    switch (get_trigger_type()) {
      case H1200_TRIGGER_TYPE_EUCLIDEAN:
        *settings++ =   H1200_SETTING_ROOT_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_ROOT_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_ROOT_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_P_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_P_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_P_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_L_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_L_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_L_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_R_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_R_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_R_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_N_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_N_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_N_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_S_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_S_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_S_EUCLIDEAN_OFFSET;
        *settings++ =   H1200_SETTING_H_EUCLIDEAN_LENGTH;
        *settings++ =   H1200_SETTING_H_EUCLIDEAN_FILL;
        *settings++ =   H1200_SETTING_H_EUCLIDEAN_OFFSET;
       break;
     default:
      break;
    }
    
    num_enabled_settings_ = settings - enabled_settings_;
  }

private:
  int num_enabled_settings_;
  H1200Setting enabled_settings_[H1200_SETTING_LAST];
  
};

const char * const output_mode_names[] = {
  "chord",
  "tune"
};

const char * const trigger_mode_names[] = {
  "P>L>R",
  "L>R>P",
  "R>P>L",
  "P>R>L",
  "R>L>P",
  "L>P>R",
};

const char * const trigger_type_names[] = {
  "PLR",
  "Eucl",
};

const char * const mode_names[] = {
  "maj", "min"
};

SETTINGS_DECLARE(H1200Settings, H1200_SETTING_LAST) {
  {0, -11, 11, "Transpose", NULL, settings::STORAGE_TYPE_I8},
  {0, -3, 3, "Octave", NULL, settings::STORAGE_TYPE_I8},
  {MODE_MAJOR, 0, MODE_LAST-1, "Root mode", mode_names, settings::STORAGE_TYPE_U8},
  {0, -3, 3, "Inversion", NULL, settings::STORAGE_TYPE_I8},
  {TRANSFORM_PRIO_XPLR, 0, TRANSFORM_PRIO_LAST-1, "Priority", trigger_mode_names, settings::STORAGE_TYPE_U8},
  {OUTPUT_CHORD_VOICING, 0, OUTPUT_MODE_LAST-1, "Output mode", output_mode_names, settings::STORAGE_TYPE_U8},
  {H1200_TRIGGER_TYPE_PLR, 0, H1200_TRIGGER_TYPE_LAST-1, "Trigger type", trigger_type_names, settings::STORAGE_TYPE_U4},
  { 8, 2, 32, ".Root Eucl len", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 4, 0, 32, ".Root Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, 32, ".Root Eucl off", NULL, settings::STORAGE_TYPE_U8 },
  { 32, 2, 32, ".P Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 2, 0, 32, ".P Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 1, 0, 32, ".P Eucl offset", NULL, settings::STORAGE_TYPE_U8 },
  { 32, 2, 32, ".L Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 3, 0, 32, ".L Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 0, 0, 32, ".L Eucl offset", NULL, settings::STORAGE_TYPE_U8 },
  { 32, 2, 32, ".R Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 1, 0, 32, ".R Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 3, 0, 32, ".R Eucl offset", NULL, settings::STORAGE_TYPE_U8 },  
  { 12, 2, 32, ".N Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 3, 0, 32, ".N Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 1, 0, 32, ".N Eucl offset", NULL, settings::STORAGE_TYPE_U8 },
  { 12, 2, 32, ".S Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 3, 0, 32, ".S Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 3, 0, 32, ".S Eucl offset", NULL, settings::STORAGE_TYPE_U8 },
  { 12, 2, 32, ".H Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
  { 5, 0, 32, ".H Eucl fill", NULL, settings::STORAGE_TYPE_U8 },
  { 5, 0, 32, "..H Eucl offset", NULL, settings::STORAGE_TYPE_U8 },  
};

static constexpr uint32_t TRIGGER_MASK_TR1 = OC::DIGITAL_INPUT_1_MASK;
static constexpr uint32_t TRIGGER_MASK_P = OC::DIGITAL_INPUT_2_MASK;
static constexpr uint32_t TRIGGER_MASK_L = OC::DIGITAL_INPUT_3_MASK;
static constexpr uint32_t TRIGGER_MASK_R = OC::DIGITAL_INPUT_4_MASK;
static constexpr uint32_t TRIGGER_MASK_DIRTY = 0x10;
static constexpr uint32_t TRIGGER_MASK_RESET = TRIGGER_MASK_TR1 | TRIGGER_MASK_DIRTY;

namespace H1200 {
  enum UserActions {
    ACTION_FORCE_UPDATE,
    ACTION_MANUAL_RESET
  };

  typedef uint32_t UiAction;
};

class H1200State {
public:
  static constexpr int kMaxInversion = 3;

  void Init() {
    cursor.Init(H1200_SETTING_ROOT_OFFSET, H1200_SETTING_LAST - 1);
    display_notes = true;

    quantizer.Init();
    tonnetz_state.init();

    euclidean_counter_ = 0;
    root_sample_ = false;
    root_ = 0;
    
  }

  void force_update() {
    ui_actions.Write(H1200::ACTION_FORCE_UPDATE);
  }

  void manual_reset() {
    ui_actions.Write(H1200::ACTION_MANUAL_RESET);
  }

  void Render(int32_t root, int inversion, int octave, OutputMode output_mode) {
    tonnetz_state.render(root + octave * 12, inversion);

    switch (output_mode) {
    case OUTPUT_CHORD_VOICING: {
      OC::DAC::set_semitone<DAC_CHANNEL_A>(tonnetz_state.outputs(0), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_B>(tonnetz_state.outputs(1), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_C>(tonnetz_state.outputs(2), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_D>(tonnetz_state.outputs(3), 0);
    }
    break;
    case OUTPUT_TUNE: {
      OC::DAC::set_semitone<DAC_CHANNEL_A>(tonnetz_state.outputs(0), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_B>(tonnetz_state.outputs(0), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_C>(tonnetz_state.outputs(0), 0);
      OC::DAC::set_semitone<DAC_CHANNEL_D>(tonnetz_state.outputs(0), 0);
    }
    break;
    default: break;
    }
  }

  menu::ScreenCursor<menu::kScreenLines> cursor;
  bool display_notes;

  inline int cursor_pos() const {
    return cursor.cursor_pos();
  }

  OC::SemitoneQuantizer quantizer;
  TonnetzState tonnetz_state;
  util::RingBuffer<H1200::UiAction, 4> ui_actions;
  uint32_t euclidean_counter_;
  bool root_sample_ ;
  int32_t root_ ;
};

H1200Settings h1200_settings;
H1200State h1200_state;

void FASTRUN H1200_clock(uint32_t triggers) {
  // Reset has priority
  if (triggers & TRIGGER_MASK_TR1) {
    h1200_state.tonnetz_state.reset(h1200_settings.mode());
  }
  if (h1200_settings.get_trigger_type() == H1200_TRIGGER_TYPE_PLR) {
    
      // Since there can be simultaneous triggers, there is a definable priority.
      // Reset always has top priority
      //
      // Note: Proof-of-concept code, do not copy/paste all combinations ;)
      switch (h1200_settings.get_transform_priority()) {
        case TRANSFORM_PRIO_XPLR:
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          break;
    
        case TRANSFORM_PRIO_XLRP:
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          break;
    
        case TRANSFORM_PRIO_XRPL:
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          break;
    
        case TRANSFORM_PRIO_XPRL:
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          break;
    
        case TRANSFORM_PRIO_XRLP:
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          break;
    
        case TRANSFORM_PRIO_XLPR:
          if (triggers & TRIGGER_MASK_L) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (triggers & TRIGGER_MASK_P) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (triggers & TRIGGER_MASK_R) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          break;
    
        default: break;
      }
  } else {

    if (triggers & TRIGGER_MASK_P) {
      ++h1200_state.euclidean_counter_;
      
      if (EuclideanFilter(h1200_settings.get_root_euclidean_length(), h1200_settings.get_root_euclidean_fill(), h1200_settings.get_root_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.root_sample_ = true;

      switch (h1200_settings.get_transform_priority()) {
        case TRANSFORM_PRIO_XPLR:
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          break;   
        case TRANSFORM_PRIO_XLRP:
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          break;   
        case TRANSFORM_PRIO_XRPL:
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          break;   
        case TRANSFORM_PRIO_XPRL:
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          break;   
        case TRANSFORM_PRIO_XRLP:
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          break;   
        case TRANSFORM_PRIO_XLPR:
          if (EuclideanFilter(h1200_settings.get_l_euclidean_length(), h1200_settings.get_l_euclidean_fill(), h1200_settings.get_l_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_L);
          if (EuclideanFilter(h1200_settings.get_p_euclidean_length(), h1200_settings.get_p_euclidean_fill(), h1200_settings.get_p_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_P);
          if (EuclideanFilter(h1200_settings.get_r_euclidean_length(), h1200_settings.get_r_euclidean_fill(), h1200_settings.get_r_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_R);
          break;
    
        default: break;
      }  
      if (EuclideanFilter(h1200_settings.get_n_euclidean_length(), h1200_settings.get_n_euclidean_fill(), h1200_settings.get_n_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_N);
      if (EuclideanFilter(h1200_settings.get_s_euclidean_length(), h1200_settings.get_s_euclidean_fill(), h1200_settings.get_s_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_S);
      if (EuclideanFilter(h1200_settings.get_h_euclidean_length(), h1200_settings.get_h_euclidean_fill(), h1200_settings.get_h_euclidean_offset(), h1200_state.euclidean_counter_)) h1200_state.tonnetz_state.apply_transformation(tonnetz::TRANSFORM_H);
        
    }
 }

  // Skip the quantizer since we just want semitones
  // but only if there is a trigger
  int transpose = h1200_settings.root_offset() ;
  if (true) {
    transpose += ((OC::ADC::value<ADC_CHANNEL_3>() + 63) >> 7);
    CONSTRAIN(transpose, -24, 24);
  }
  int32_t root = transpose;
  if (h1200_state.root_sample_) {
      h1200_state.root_sample_ = false;
      root += h1200_state.quantizer.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_1)) ;
      h1200_state.root_ = root;
  }
  int octave = h1200_settings.octave() + ((OC::ADC::value<ADC_CHANNEL_2>() + 511) >> 10);
  CONSTRAIN(octave, -3, 3);

  int inversion = h1200_settings.inversion() + ((OC::ADC::value<ADC_CHANNEL_4>() + 255) >> 9);
  CONSTRAIN(inversion, -H1200State::kMaxInversion, H1200State::kMaxInversion);

  h1200_state.Render(h1200_state.root_, inversion, octave, h1200_settings.output_mode());

  if (triggers)
    MENU_REDRAW = 1;
}

void H1200_init() {
  h1200_settings.Init();
  h1200_state.Init();
  h1200_settings.update_enabled_settings();
  h1200_state.cursor.AdjustEnd(h1200_settings.num_enabled_settings() - 1);
}

size_t H1200_storageSize() {
  return H1200Settings::storageSize();
}

size_t H1200_save(void *storage) {
  return h1200_settings.Save(storage);
}

size_t H1200_restore(const void *storage) {
  h1200_settings.update_enabled_settings();
  h1200_state.cursor.AdjustEnd(h1200_settings.num_enabled_settings() - 1);
  return h1200_settings.Restore(storage);
}

void H1200_handleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      h1200_state.cursor.set_editing(false);
      h1200_state.tonnetz_state.reset(h1200_settings.mode());
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      h1200_settings.update_enabled_settings();
      h1200_state.cursor.AdjustEnd(h1200_settings.num_enabled_settings() - 1);
      break;
  }
}

void H1200_isr() {
  uint32_t triggers = OC::DigitalInputs::clocked();

  while (h1200_state.ui_actions.readable()) {
    switch (h1200_state.ui_actions.Read()) {
      case H1200::ACTION_FORCE_UPDATE:
        triggers |= TRIGGER_MASK_DIRTY;
        break;
      case H1200::ACTION_MANUAL_RESET:
        triggers |= TRIGGER_MASK_RESET;
        break;
      default:
        break;
    }
  }

  H1200_clock(triggers);
}

void H1200_loop() {
}

void H1200_handleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        if (h1200_settings.change_value(H1200_SETTING_INVERSION, 1))
          h1200_state.force_update();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        if (h1200_settings.change_value(H1200_SETTING_INVERSION, -1))
          h1200_state.force_update();
        break;
      case OC::CONTROL_BUTTON_L:
        h1200_state.display_notes = !h1200_state.display_notes;
        break;
      case OC::CONTROL_BUTTON_R:
        h1200_state.cursor.toggle_editing();
        break;
    }
  } else {
    if (OC::CONTROL_BUTTON_L == event.control) {
      h1200_settings.InitDefaults();
      h1200_state.manual_reset();
    }
  }
}

void H1200_handleEncoderEvent(const UI::Event &event) {

  if (OC::CONTROL_ENCODER_L == event.control) {
    if (h1200_settings.change_value(H1200_SETTING_ROOT_OFFSET, event.value))
      h1200_state.force_update();
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (h1200_state.cursor.editing()) {
      H1200Setting setting = h1200_settings.enabled_setting_at(h1200_state.cursor_pos());
      
      if (h1200_settings.change_value(h1200_state.cursor.cursor_pos(), event.value))
        h1200_state.force_update();

          switch(setting) {
  
            case H1200_SETTING_TRIGGER_TYPE:
              h1200_settings.update_enabled_settings();
              h1200_state.cursor.AdjustEnd(h1200_settings.num_enabled_settings() - 1);
            break;
            default:
            break;
          }
        
    } else {
      h1200_state.cursor.Scroll(event.value);
    }
  }
}

void H1200_menu() {

  const EMode current_mode = h1200_state.tonnetz_state.current_chord().mode();
  int outputs[4];
  h1200_state.tonnetz_state.get_outputs(outputs);

  menu::DefaultTitleBar::Draw();
  graphics.print(note_name(outputs[0]));
  graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
  graphics.print(mode_names[current_mode]);
  graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);

  if (h1200_state.display_notes) {
    for (size_t i=1; i < 4; ++i) {
      graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
      graphics.print(note_name(outputs[i]));
    }
  } else {
    for (size_t i=1; i < 4; ++i) {
      graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
      graphics.pretty_print(outputs[i]);
    }
  }

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(h1200_state.cursor);
  menu::SettingsListItem list_item;
  while (settings_list.available()) {
    const int current = h1200_settings.enabled_setting_at(settings_list.Next(list_item));
    list_item.DrawDefault(h1200_settings.get_value(current), H1200Settings::value_attr(current));
  }
}

void H1200_screensaver() {
  uint8_t y = 0;
  static const uint8_t x_col_0 = 66;
  static const uint8_t x_col_1 = 66 + 24;
  static const uint8_t line_h = 16;
  static const weegfx::coord_t note_circle_x = 32;
  static const weegfx::coord_t note_circle_y = 32;

  uint32_t history = h1200_state.tonnetz_state.history();
  int outputs[4];
  h1200_state.tonnetz_state.get_outputs(outputs);

  uint8_t normalized[3];
  y = 8;
  for (size_t i=0; i < 3; ++i, y += line_h) {
    int note = outputs[i + 1];
    int octave = note / 12;
    note = (note + 120) % 12;
    normalized[i] = note;

    graphics.setPrintPos(x_col_1, y);
    graphics.print(OC::Strings::note_names_unpadded[note]);
    graphics.print(octave + 1);
  }
  y = 0;

  size_t len = 4;
  while (len--) {
    graphics.setPrintPos(x_col_0, y);
    graphics.print(history & 0x80 ? '+' : '-');
    graphics.print(tonnetz::transform_names[static_cast<tonnetz::ETransformType>(history & 0x7f)]);
    y += line_h;
    history >>= 8;
  }

  OC::visualize_pitch_classes(normalized, note_circle_x, note_circle_y);
}

void H1200_debug() {
  int cv = OC::ADC::value<ADC_CHANNEL_4>();
  int scaled = ((OC::ADC::value<ADC_CHANNEL_4>() + 255) >> 9);

  graphics.setPrintPos(2, 12);
  graphics.printf("I: %4d %4d", cv, scaled);
}
