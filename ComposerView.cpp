#include "ComposerView.h"

#include <vector>
#include <string_view>
#include <algorithm>
#include <mutex>

#include "imgui.h"
#include "logging.h"
#include "soil.h"
#include "Sequencer.h"
#include "SDL.h"
#include "GlobalRenderData.h"
#include "ShaderProgram.h"
#include "Mixer.h"

static constexpr float kFullBeatWidth = 80.0f;
static constexpr float kKeyboardKeyWidth = 100.0f;
static constexpr float kKeyboardKeyHeight = 20.0f;
static constexpr uint32 kDefaultBpm = 120;
static constexpr uint32 kDefaultNumMeasures = 2;
static constexpr uint32 kDefaultBeatsPerMeasure = 4;
static constexpr uint32 kDefaultSubdivisions = 4;
static constexpr float kPlayNoteFlashDuration = 0.5f;
static constexpr float kPlayNoteFlashGrow = 1.0f;
static constexpr uint32 kPlayNoteFlashColor = 0x0000FFFF;
static constexpr uint32 kMaxMeasures = 256;
static constexpr uint32 kMinMeasures = 1;
static constexpr uint32 kMaxBeatsPerMeasure = 12;
static constexpr uint32 kMinBeatsPerMeasure = 2;
static constexpr float kDefaultNoteVelocity = 1.0f;
static constexpr uint32 kPlayTrackFlashColor = 0x00007F7F;
static constexpr float kPlayTrackFlashDuration = 0.5f;
static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;
static constexpr float kScrollBarWidth = 15.0f;
static constexpr float kSequencerWindowToolbarHeight = 64.0f;

// 32 divisions per beat, viewable as 1/2,1/4,1/8,1/16
static const std::vector<uint32> TimelineDivisions = { 2, 4, 8 };

#define HOOBASTANK

void ComposerView::OutputWindowState::ClearLog() {
  displayHistory.clear();
}

void ComposerView::OutputWindowState::AddLog(const char* fmt, ...)
{
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
  buf[IM_ARRAYSIZE(buf) - 1] = 0;
  va_end(args);
  this->displayHistory.emplace_back(buf);
  if (autoScroll) {
    scrollToBottom = true;
  }
}

void ComposerView::OutputWindowState::AddLog(const std::string_view& logString) {
  displayHistory.push_back(std::string(logString));
  if (autoScroll) {
    scrollToBottom = true;
  }
}


ComposerView::ImGuiRenderable::ImGuiRenderable() {
  // Generate ImGui VB and EB
  glGenBuffers(1, &vertexBufferId);
  glGenBuffers(1, &elementBufferId);

  // Get our shader program
  shaderProgram = GlobalRenderData::get().getShaderProgram("ImGuiProgram");
  if (shaderProgram != nullptr) {
    // Get the world/scale uniform (TODO: abstract this)
    fontTextureLoc = glGetUniformLocation(shaderProgram->getProgramId(), "Texture");

    // Bind vertex attributes
    addShaderAttribute(shaderProgram->getProgramId(),
      "Position", 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, pos));
    addShaderAttribute(shaderProgram->getProgramId(),
      "UV", 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, uv));
    addShaderAttribute(shaderProgram->getProgramId(),
      "Color", 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, col));
  }
  else {
    std::string strError("ComposerView::ImGuiRenderable: Unable to find loaded shader program");
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }
}

void ComposerView::ImGuiRenderable::Render() {
  if (shaderProgram != nullptr) {
    ImGui::Render();

    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Backup GL state
    GLboolean lastEnableBlend = glIsEnabled(GL_BLEND);
    GLboolean lastEnableCullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean lastEnableDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean lastEnableScissorTest = glIsEnabled(GL_SCISSOR_TEST);

    GLint lastTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    GLint lastArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);
    GLint lastElementArrayBuffer;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER, &lastElementArrayBuffer);

    // Set our state
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    auto& imGuiIo = ImGui::GetIO();
    glViewport(0, 0, static_cast<GLuint>(imGuiIo.
      DisplaySize.x), static_cast<GLuint>(imGuiIo.DisplaySize.y));

    // Handle issue where screen coordinates and framebuffer coordinates are not 1:1
    auto frameBufferHeight = imGuiIo.DisplaySize.y * imGuiIo.DisplayFramebufferScale.y;
    imDrawData->ScaleClipRects(imGuiIo.DisplayFramebufferScale);

    shaderProgram->begin();

    // Select the 0th texture
    glUniform1i(fontTextureLoc, 0);

    // Run through the ImGui draw lists
    for (int n = 0; n < imDrawData->CmdListsCount; ++n) {
      const auto& cmdList = imDrawData->CmdLists[n];

      // Bind vertex buffer and set vertex data
      glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->VtxBuffer.size()) *
        sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(&cmdList->VtxBuffer.front()), GL_STREAM_DRAW);

      // Bind buffer offsets to vertex offsets
      bindShaderAttributes(sizeof(ImDrawVert));

      // Bind index buffer and set index data
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->IdxBuffer.size()) *
        sizeof(ImDrawIdx), reinterpret_cast<GLvoid*>(&cmdList->IdxBuffer.front()), GL_STREAM_DRAW);

      // Run through the draw commands for this draw list
      const ImDrawIdx* drawIndex = 0;
      for (const auto& drawCmd : cmdList->CmdBuffer) {
        if (drawCmd.UserCallback != nullptr) {
          drawCmd.UserCallback(cmdList, &drawCmd);
        }
        else {
          glBindTexture(GL_TEXTURE_2D, reinterpret_cast<GLuint>(drawCmd.TextureId));
          glScissor(static_cast<int>(drawCmd.ClipRect.x),
            static_cast<int>(frameBufferHeight - drawCmd.ClipRect.w),
            static_cast<int>(drawCmd.ClipRect.z - drawCmd.ClipRect.x),
            static_cast<int>(drawCmd.ClipRect.w - drawCmd.ClipRect.y));
          glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawCmd.ElemCount), GL_UNSIGNED_SHORT, drawIndex);
        }
        drawIndex += drawCmd.ElemCount;
      }
    }

    shaderProgram->end();

    // Restore previous GL state
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lastElementArrayBuffer);

    if (lastEnableBlend) {
      glEnable(GL_BLEND);
    }
    else {
      glDisable(GL_BLEND);
    }
    if (lastEnableCullFace) {
      glEnable(GL_CULL_FACE);
    }
    else {
      glDisable(GL_CULL_FACE);
    }
    if (lastEnableDepthTest) {
      glEnable(GL_DEPTH_TEST);
    }
    else {
      glDisable(GL_DEPTH_TEST);
    }
    if (lastEnableScissorTest) {
      glEnable(GL_SCISSOR_TEST);
    }
    else {
      glDisable(GL_SCISSOR_TEST);
    }
  }
}

void ComposerView::ImGuiRenderable::SetTrackColors(std::string colorScheme, uint32& flashColor) {

  if (colorScheme.length()) {
    auto& imGuiStyle = ImGui::GetStyle();

    std::transform(colorScheme.begin(), colorScheme.end(), colorScheme.begin(), ::tolower);

    if (colorScheme == "piano:white") {
      imGuiStyle.Colors[ImGuiCol_Button] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
      imGuiStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.5f);
      imGuiStyle.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
      flashColor = 0x00666680;
    }
    if (colorScheme == "piano:black") {
      imGuiStyle.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
      imGuiStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.7f, 0.7f, 0.5f);
      imGuiStyle.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      flashColor = 0x00838380;
    }
  }
}

void ComposerView::Render(double currentTime, ImVec2 canvasSize) {
  auto& sequencer = Sequencer::Get();

  // Lock the instrument for the duration
  //std::lock_guard<std::mutex> lockInstrument(mutexInstrument);
  auto instrument = sequencer.GetInstrument();

  // Lock out the audio callback to update the shared data
  SDL_LockAudio();
  playingTrackFlashTimes[0] = playingTrackFlashTimes[1];
  playingNotesFlashTimes[0] = playingNotesFlashTimes[1];

  if (pendingPlayTrack != -1) {
    sequencer.PlayInstrumentTrack(pendingPlayTrack, kDefaultNoteVelocity);
    pendingPlayTrack = -1;
  }
  SDL_UnlockAudio();

  int outputWindowHeight = canvasSize.y * kOutputWindowWindowScreenHeightPercentage;
  int sequencerHeight = canvasSize.y - outputWindowHeight;

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(sequencerHeight)));
  ImGui::Begin("Instrument",
    nullptr,
    //ImGuiWindowFlags_NoTitleBar | 
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
  {
    auto& imGuiStyle = ImGui::GetStyle();
    auto oldItemSpacing = imGuiStyle.ItemSpacing;

    if (instrument != nullptr) {
      ImGui::Text("Instrument: %s", instrument->GetName().c_str());
    }
    else {
      ImGui::Text("Instrument: None");
    }

    ImGui::NewLine();
    ImGui::Separator();

    auto beatWidth = kFullBeatWidth / sequencer.GetSubdivision();

    // Start of the beat label
    float beatLabelStartX = ImGui::GetCursorPosX() + kKeyboardKeyWidth + imGuiStyle.ItemSpacing.x;

    // Beat numbers
    float cursorPosX = beatLabelStartX;
    for (size_t b = 0; b < sequencer.GetNumMeasures() * sequencer.GetBeatsPerMeasure(); ++b) {
      ImGui::SetCursorPosX(cursorPosX);
      cursorPosX += kFullBeatWidth;
      ImGui::Text(std::to_string(b + 1).c_str());
      ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    auto beatLabelStartY = ImGui::GetCursorPosY();

    // Tracks
    ImGui::BeginChild("##InstrumentScrollingRegion",
      ImVec2(static_cast<float>(canvasSize.x) - kScrollBarWidth,
        static_cast<float>(sequencerHeight) - kSequencerWindowToolbarHeight - beatLabelStartY),
      false,
      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    {
      if (instrument != nullptr) {
        uint32 noteGlobalIndex = 0;
        for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
          auto& track = instrument->GetTracks()[trackIndex];

          // Track label and UI button for manual trigger
          ImVec4 oldColors[ImGuiCol_COUNT];
          memcpy(oldColors, imGuiStyle.Colors, sizeof(oldColors));

          uint32 flashColor = kPlayTrackFlashColor;
          imGuiRenderable.SetTrackColors(track.GetColorScheme(), flashColor);

          auto prevPos = ImGui::GetCursorPos();
          if (ImGui::Button(track.GetName().
            c_str(), ImVec2(kKeyboardKeyWidth, kKeyboardKeyHeight))) {
            // Do it next frame so we only lock audio once per UI loop
            pendingPlayTrack = trackIndex;
          }
          auto currPos = ImGui::GetCursorPos();

          // If it's playing, flash it
          float flashPct = 0.0f;
          {
            auto flashTime = playingTrackFlashTimes[0].find(trackIndex);
            if (flashTime != playingTrackFlashTimes[0].end()) {
              auto pct = static_cast<float>((currentTime - flashTime->second) / kPlayTrackFlashDuration);
              if (pct >= 1.0f) {
                playingTrackFlashTimes[0].erase(flashTime);
              }
              else {
                flashPct = 1.0f - pct;
              }
            }
          }

          if (flashPct > 0.0f) {
            prevPos.x -= kPlayNoteFlashGrow * flashPct;
            prevPos.y -= kPlayNoteFlashGrow * flashPct;

            ImGui::SetCursorPos(prevPos);
            ImGui::FillRect(ImVec2(kKeyboardKeyWidth + kPlayNoteFlashGrow * 2.0f * flashPct,
              kKeyboardKeyHeight + kPlayNoteFlashGrow * 2.0f * flashPct),
              (static_cast<uint32>(flashPct * 255.0f) << 24) | flashColor);
            ImGui::SetCursorPos(currPos);
          }

          memcpy(imGuiStyle.Colors, oldColors, sizeof(oldColors));

          // Beat groups
          uint32 noteLocalIndex = 0;
          for (size_t b = 0; b < sequencer.GetNumMeasures() * sequencer.GetBeatsPerMeasure(); ++b) {
            // Notes
            for (size_t s = 0; s < sequencer.GetSubdivision(); ++s) {
              ImGui::SameLine();

              // Lesson learned: labels are required to pair UI with UX
              auto uniqueLabel(track.GetName() + std::string("#") + std::to_string(noteLocalIndex));

              auto trackNote = track.GetNotes()[noteLocalIndex];
              auto cursorPos = ImGui::GetCursorPos();

              // Toggle notes that are clicked
              imGuiStyle.ItemSpacing.x = 0.0f;
              imGuiStyle.ItemSpacing.y = 0.0f;
              if (ImGui::SquareRadioButton(uniqueLabel.c_str(), trackNote != 0, beatWidth, kKeyboardKeyHeight)) {
                if (trackNote != 0) {
                  trackNote = 0;
                }
                else {
                  trackNote = kDefaultNoteVelocity;
                }

                // Can only set notes via the sequencer
                sequencer.SetTrackNote(trackIndex, noteLocalIndex, trackNote);
              }

              // Draw filled note
              if (trackNote != 0) {
                auto currentPos = ImGui::GetCursorPos();

                ImGui::SetCursorPos(cursorPos);
                ImGui::FillRect(ImVec2(beatWidth, kKeyboardKeyHeight), 0xFFFFFFFF);

                // If it's playing, flash it
                float flashPct = 0.0f;
                {
                  auto flashTime = playingNotesFlashTimes[0].find(noteGlobalIndex);
                  if (flashTime != playingNotesFlashTimes[0].end()) {
                    auto pct = static_cast<float>((currentTime - flashTime->second) / kPlayNoteFlashDuration);
                    if (pct >= 1.0f) {
                      playingNotesFlashTimes[0].erase(flashTime);
                    }
                    else {
                      flashPct = 1.0f - pct;
                    }
                  }
                }

                if (flashPct > 0.0f) {
                  auto flashCursorPos = cursorPos;

                  flashCursorPos.x -= kPlayNoteFlashGrow * flashPct;
                  flashCursorPos.y -= kPlayNoteFlashGrow * flashPct;

                  ImGui::SetCursorPos(flashCursorPos);
                  ImGui::FillRect(ImVec2(beatWidth + kPlayNoteFlashGrow * 2.0f * flashPct,
                    kKeyboardKeyHeight + kPlayNoteFlashGrow * 2.0f * flashPct),
                    (static_cast<uint32>(flashPct * 255.0f) << 24) | kPlayNoteFlashColor);
                }

                ImGui::SetCursorPos(currentPos);
              }

              noteLocalIndex += sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision();
              noteGlobalIndex += sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision();
            }
          }

          // Reset old X spacing to offset from keyboard key
          imGuiStyle.ItemSpacing.x = oldItemSpacing.x;
        }
      }
    }
    ImGui::EndChild();

    imGuiStyle.ItemSpacing = oldItemSpacing;

    float beatLabelEndY = ImGui::GetCursorPosY() - imGuiStyle.ItemSpacing.y;

    ImGui::Separator();
    ImGui::NewLine();

    // Bottom tool bar
    {
      imGuiStyle.ItemSpacing.x = 0;

      // Play/Pause button
      if (sequencer.IsPlaying()) {
        if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(pauseButtonIconTexture), ImVec2(20, 20))) {
          sequencer.Pause();
        }
      }
      else {
        if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(playButtonIconTexture), ImVec2(20, 20))) {
          sequencer.Play();
        }
      }

      ImGui::SameLine();

      // Stop button
      if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(stopButtonIconTexture), ImVec2(20, 20))) {
        sequencer.Stop();
      }

      imGuiStyle.ItemSpacing.x = oldItemSpacing.x + 5;

      // Measures
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int numMeasures = sequencer.GetNumMeasures();
      if (ImGui::InputInt("Measures", &numMeasures)) {
        numMeasures = std::max(std::min(static_cast<uint32>(numMeasures), kMaxMeasures), kMinMeasures);
        sequencer.SetNumMeasures(numMeasures);
      }
      ImGui::PopItemWidth();

      // Beats per measure
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int beatsPerMeasure = sequencer.GetBeatsPerMeasure();
      if (ImGui::InputInt("BeatsPerMeasure", &beatsPerMeasure)) {
        beatsPerMeasure = std::max(std::min(static_cast<uint32>
          (beatsPerMeasure), kMaxBeatsPerMeasure), kMinBeatsPerMeasure);
        sequencer.SetBeatsPerMeasure(beatsPerMeasure);
      }
      ImGui::PopItemWidth();

      // Subdivision
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      if (ImGui::BeginCombo("Subdivision", std::to_string(sequencer.GetSubdivision()).c_str())) {
        for (size_t s = 0; s < TimelineDivisions.size(); ++s) {
          bool isSelected = (sequencer.GetSubdivision() == TimelineDivisions[s]);
          if (ImGui::Selectable(std::to_string(TimelineDivisions[s]).c_str(), isSelected)) {
            sequencer.SetSubdivision(TimelineDivisions[s]);
          }
          else {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();

      // BPM
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int currentBpm = sequencer.GetBeatsPerMinute();
      if (ImGui::InputInt("BPM", &currentBpm)) {
        sequencer.SetBeatsPerMinute(currentBpm);
      }
      ImGui::PopItemWidth();

      // Loop
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool isLooping = sequencer.IsLooping();
      if (ImGui::Checkbox("Loop", &isLooping)) {
        sequencer.SetLooping(isLooping);
      }
      ImGui::PopItemWidth();

      // Metronome
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool isMetronomeOn = sequencer.IsMetronomeOn();
      if (ImGui::Checkbox("Metronome", &isMetronomeOn)) {
        sequencer.EnableMetronome(isMetronomeOn);
      }
      ImGui::PopItemWidth();
    }

    auto oldCursorPos = ImGui::GetCursorPos();

    // Draw the beat demarcation lines
    cursorPosX = beatLabelStartX;
    for (uint32 b = 0; b < sequencer.GetNumMeasures() * sequencer.GetBeatsPerMeasure(); ++b) {
      ImGui::SetCursorPos(ImVec2(cursorPosX - 0, beatLabelStartY));
      ImGui::FillRect(ImVec2(1, beatLabelEndY -
        beatLabelStartY), ImGui::GetColorU32(ImGuiCol_FrameBgActive));
      cursorPosX += kFullBeatWidth;
    }

    imGuiStyle.ItemSpacing = oldItemSpacing;

    // Draw the play line
    cursorPosX = beatLabelStartX + beatWidth * (sequencer.GetPosition() /
      (sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision()));
    ImGui::SetCursorPos(ImVec2(cursorPosX - 0, beatLabelStartY));
    ImGui::FillRect(ImVec2(1, beatLabelEndY - beatLabelStartY), 0x7FFFFFFF);

    ImGui::SetCursorPos(oldCursorPos);
  }
  ImGui::End();

  // Output window
  int outputWindowTop = canvasSize.y - outputWindowHeight;
  ImGui::SetNextWindowPos(ImVec2(0, outputWindowTop));
  ImGui::SetNextWindowSize(ImVec2(canvasSize.x, outputWindowHeight));
  ImGui::Begin("Output",
    nullptr,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
  {
    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
      if (ImGui::MenuItem("Clear")) {
        outputWindowState.ClearLog();
      }
      if (ImGui::MenuItem("Scroll to bottom")) {
        outputWindowState.scrollToBottom = true;
      }
      bool autoScroll = outputWindowState.autoScroll;
      if (ImGui::Checkbox("Auto-scroll", &autoScroll)) {
        outputWindowState.autoScroll = autoScroll;
        if (autoScroll) {
          outputWindowState.scrollToBottom = true;
        }
      }
      ImGui::EndPopup();
    }

    if (ImGui::SmallButton("=")) {
      ImGui::OpenPopup("Options");
    }
    ImGui::SameLine();
    ImGui::Text("Voices: %d", Mixer::Get().GetNumActiveVoices());
    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
      // OutputWindow stuff
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

      for (const auto& historyText : outputWindowState.displayHistory) {
        if (historyText.length() < 1) {
          continue;
        }

        // TODO: rather than parse the string, create an expanded class
        static const ImVec4 logColors[Logging::Category::Count] = {
          ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // Info = White
          ImVec4(0.7f, 0.7f, 0.0f, 1.0f), // Warn = Yellow
          ImVec4(0.7f, 0.0f, 0.0f, 1.0f), // Error = Red
          ImVec4(1.0f, 0.0f, 0.0f, 1.0f), // Fatal = Bright Red
        };

        auto logCategory = static_cast<Logging::Category>(*historyText.c_str() - 1);
        ImGui::PushStyleColor(ImGuiCol_Text, logColors[logCategory]);
        ImGui::TextUnformatted(historyText.c_str() + 1);
        ImGui::PopStyleColor();
      }
    }
    if (outputWindowState.scrollToBottom) {
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
      ImGui::SetScrollHere(1.0f);
      outputWindowState.scrollToBottom = false;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
  }

  ImGui::End();

  imGuiRenderable.Render();
}

void ComposerView::InitResources() {
  auto& imGuiIo = ImGui::GetIO();

  // Backup GL state
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

  // Create GL texture for font
  glGenTextures(1, &fontTextureId);
  glBindTexture(GL_TEXTURE_2D, fontTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  uchar* pixels;
  int width, height;
  imGuiIo.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  imGuiIo.Fonts->TexID = reinterpret_cast<ImTextureID>(fontTextureId);
  imGuiIo.Fonts->ClearInputData();
  imGuiIo.Fonts->ClearTexData();

  // Load UI textures
  glGenTextures(1, &playButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, playButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  uint8* iconData = SOIL_load_image("Assets\\icon_play.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  glGenTextures(1, &pauseButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, pauseButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\icon_pause.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  // Set font texture ID
  glGenTextures(1, &stopButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, stopButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\icon_stop.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  // Restore original state
  glBindTexture(GL_TEXTURE_2D, lastTexture);
}

ComposerView::ComposerView() {
  logResponderId = Logging::AddResponder([=](const std::string_view& logLine) {
    outputWindowState.AddLog(logLine);
  });

  InitResources();
}

ComposerView::~ComposerView() {
  if (logResponderId != UINT32_MAX) {
    Logging::PopResponder(logResponderId);
    logResponderId = UINT32_MAX;
  }
}