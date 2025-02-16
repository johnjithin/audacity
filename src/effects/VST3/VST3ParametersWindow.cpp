/**********************************************************************

  Audacity: A Digital Audio Editor

  @file VST3ParametersWindow.cpp

  @author Vitaly Sverchinsky

  @brief Part of Audacity VST3 module

**********************************************************************/


#include "VST3ParametersWindow.h"

#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/slider.h>

#include "MemoryX.h"
#include "VST3Utils.h"
#if wxUSE_ACCESSIBILITY
#include "../../widgets/WindowAccessible.h"
#endif


//Interface between wx control and IEditController parameter.
class VST3ParameterControl
{
   const Steinberg::Vst::ParamID mParameterId;
public:
   VST3ParameterControl(Steinberg::Vst::ParamID id) : mParameterId(id) { }

   virtual ~VST3ParameterControl() { }
   //Read parameter value from the controller and update UI
   virtual void SetNormalizedValue(Steinberg::Vst::IEditController&, Steinberg::Vst::ParamValue value) = 0;
   //Convert current control value to normalized parameter value
   virtual Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController& editController) const = 0;
   //Update a control's accessible object if necessary
   virtual void UpdateAccessible(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) {}

   Steinberg::Vst::ParamID GetParameterId() const noexcept { return mParameterId; }
};

namespace
{
   class VST3ValueText final : public wxStaticText, public VST3ParameterControl
   {
      const wxString mUnits;
   public:
      VST3ValueText(wxWindow *parent,
                 wxWindowID id,
                 Steinberg::Vst::ParamID paramId,
                 const wxString& units,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = 0,
                 const wxString& name = wxStaticTextNameStr)
      : wxStaticText(parent, id, wxEmptyString, pos, size, style, name)
      , VST3ParameterControl(paramId)
      , mUnits(units) { }

      void SetNormalizedValue(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         Steinberg::Vst::String128 str { };
         editController.getParamStringByValue(GetParameterId(), value, str);
         if(mUnits.empty())
            SetLabel(VST3Utils::ToWxString(str));
         else
            SetLabel(wxString::Format("%s %s", str, mUnits));
      }
      
      Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController& editController) const override
      {
         return editController.getParamNormalized(GetParameterId());
      }
   };

   class VST3ListParameter final : public wxChoice, public VST3ParameterControl
   {
   public:
      VST3ListParameter(wxWindow *parent,
             wxWindowID id,
             Steinberg::Vst::ParamID paramId,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxChoiceNameStr)
      : wxChoice(parent, id, pos, size, 0, nullptr, style, validator, name)
      , VST3ParameterControl(paramId) { }

      void SetNormalizedValue(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         SetSelection(static_cast<int>(editController.normalizedParamToPlain(GetParameterId(), value)));
      }

      Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController& editController) const override
      {
         return editController.plainParamToNormalized(GetParameterId(), GetSelection());
      }
   };

   class VST3ContinuousParameter final : public wxSlider, public VST3ParameterControl
   {
      const wxString mTitle;
      const wxString mUnits;

   public:
      static constexpr auto Step = 0.01;

      VST3ContinuousParameter(wxWindow *parent,
             wxWindowID id,
             Steinberg::Vst::ParamID paramId,
             const wxString& title,
             const wxString& units,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxSL_HORIZONTAL,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxSliderNameStr)
      : wxSlider(parent, id, 0, 0, static_cast<int>(1.0 / Step), pos, size, style, validator, name)
      , VST3ParameterControl(paramId)
      , mTitle(title)
      , mUnits(units)
      {
#if wxUSE_ACCESSIBILITY
         SetAccessible(safenew WindowAccessible(this));
#endif        
      }

      void SetNormalizedValue(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         SetValue(static_cast<int>(value / Step));
         UpdateAccessible(editController, value);         
      }

      void UpdateAccessible(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         Steinberg::Vst::String128 str{};
         editController.getParamStringByValue(GetParameterId(), value, str);
         SetName(wxString::Format("%s %s %s", mTitle, str, mUnits));
      }

      Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController&) const override
      {
         return GetValue() * Step;
      }
   };

   class VST3DiscreteParameter final : public wxSlider, public VST3ParameterControl
   {
      const wxString mTitle;
      const wxString mUnits;

   public:
      VST3DiscreteParameter(wxWindow *parent,
             wxWindowID id,
             Steinberg::Vst::ParamID paramId,
             int maxValue,
             const wxString& title,
             const wxString& units,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxSL_HORIZONTAL,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxSliderNameStr)
      : wxSlider(parent, id, 0, 0, maxValue, pos, size, style, validator, name)
      , VST3ParameterControl(paramId)
      , mTitle(title)
      , mUnits(units)
      {
#if wxUSE_ACCESSIBILITY
         SetAccessible(safenew WindowAccessible(this));
#endif        
      }

      void SetNormalizedValue(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         SetValue(static_cast<int>(editController.normalizedParamToPlain(GetParameterId(), value)));
         UpdateAccessible(editController, value);
      }

      void UpdateAccessible(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         Steinberg::Vst::String128 str{ };
         editController.getParamStringByValue(GetParameterId(), value, str);
         SetName(wxString::Format("%s %s %s", mTitle, str, mUnits));
      }

      Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController& editController) const override
      {
         return editController.plainParamToNormalized(GetParameterId(), GetValue());
      }
   };

   class VST3ToggleParameter final : public wxCheckBox, public VST3ParameterControl
   {
   public:

      VST3ToggleParameter(wxWindow *parent,
               wxWindowID id,
               Steinberg::Vst::ParamID paramId,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxCheckBoxNameStr)
      : wxCheckBox(parent, id, wxEmptyString, pos, size, style, validator, name)
      , VST3ParameterControl(paramId) { }

      void SetNormalizedValue(Steinberg::Vst::IEditController& editController, Steinberg::Vst::ParamValue value) override
      {
         SetValue(value != .0);
      }

      Steinberg::Vst::ParamValue GetNormalizedValue(Steinberg::Vst::IEditController&) const override
      {
         return GetValue() ? 1. : .0; 
      }
   };
}

VST3ParametersWindow::VST3ParametersWindow(wxWindow *parent,
                                           Steinberg::Vst::IEditController& editController,
                                           Steinberg::Vst::IComponentHandler& handler,
                                           wxWindowID id,
                                           const wxPoint& pos,
                                           const wxSize& size,
                                           long style,
                                           const wxString& name)
   : wxScrolledWindow(parent, id, pos, size, style, name)
   , mEditController(&editController)
   , mComponentHandler(&handler)
{
   using namespace Steinberg;

   auto sizer = std::make_unique<wxGridSizer>(3, 5, 5);

   for(int i = 0, count = editController.getParameterCount(); i < count; ++i)
   {
      Vst::ParameterInfo parameterInfo { };
      if(editController.getParameterInfo(i, parameterInfo) != kResultOk)
         continue;

      if(parameterInfo.flags & (Vst::ParameterInfo::kIsHidden | Vst::ParameterInfo::kIsProgramChange))
         continue;
      
      if((parameterInfo.flags & (Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsBypass | Vst::ParameterInfo::kIsReadOnly)) == 0)
         continue;

      sizer->Add(safenew wxStaticText(
         this,
         wxID_ANY,
         VST3Utils::ToWxString(parameterInfo.title),
         wxDefaultPosition,
         wxDefaultSize,
         wxALIGN_RIGHT), 0, wxEXPAND
      );
      
      if(parameterInfo.flags & Vst::ParameterInfo::kIsReadOnly)
      {
         auto text = safenew VST3ValueText(
            this,
            wxID_ANY,
            parameterInfo.id,
            VST3Utils::ToWxString(parameterInfo.units)
         );
         sizer->Add(text);
         sizer->AddStretchSpacer();
         RegisterParameterControl(text);
      }
      //toggle
      else if(parameterInfo.stepCount == 1)
      {
         const auto toggle = safenew VST3ToggleParameter (this, wxID_ANY, parameterInfo.id);
         toggle->Bind(wxEVT_CHECKBOX, &VST3ParametersWindow::OnParameterValueChanged, this);
         sizer->Add(toggle, 0, wxEXPAND);
         sizer->AddStretchSpacer();
         RegisterParameterControl(toggle);
      }
      //list
      else if(parameterInfo.stepCount != 0 && (parameterInfo.flags & Vst::ParameterInfo::kIsList))
      {
         const auto list = safenew VST3ListParameter(this, wxID_ANY, parameterInfo.id);

         for(auto j = 0; j <= parameterInfo.stepCount; ++j)
         {
            Vst::String128 displayValue = { 0 };
            editController.getParamStringByValue(
               parameterInfo.id,
               editController.plainParamToNormalized(parameterInfo.id, j),
               displayValue
            );
            list->AppendString(VST3Utils::ToWxString(displayValue));
         }
         list->Bind(wxEVT_CHOICE, &VST3ParametersWindow::OnParameterValueChanged, this);
         sizer->Add(list, 0, wxEXPAND);
         sizer->AddStretchSpacer();
         RegisterParameterControl(list);
      }
      else
      {
         //continuous
         if(parameterInfo.stepCount == 0)
         {
            auto slider = safenew VST3ContinuousParameter(this, wxID_ANY, parameterInfo.id,
               VST3Utils::ToWxString(parameterInfo.title), VST3Utils::ToWxString(parameterInfo.units));
            sizer->Add(slider, 0, wxEXPAND);
            slider->Bind(wxEVT_SLIDER, &VST3ParametersWindow::OnParameterValueChanged, this);
            RegisterParameterControl(slider);
         }
         //discrete
         else
         {
            auto slider = safenew VST3DiscreteParameter(this, wxID_ANY, parameterInfo.id, parameterInfo.stepCount,
               VST3Utils::ToWxString(parameterInfo.title), VST3Utils::ToWxString(parameterInfo.units));
            sizer->Add(slider, 0, wxEXPAND);
            slider->Bind(wxEVT_SLIDER, &VST3ParametersWindow::OnParameterValueChanged, this);
            RegisterParameterControl(slider);
         }
         const auto label = safenew VST3ValueText(this, wxID_ANY, parameterInfo.id, VST3Utils::ToWxString(parameterInfo.units));
         sizer->Add(label);
         RegisterParameterLabel(label);
      }
   }

   SetSizer(sizer.release());
}

void VST3ParametersWindow::ReloadParameters()
{
   for(auto& p : mControls)
      p.second->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(p.second->GetParameterId()));
   for(auto& p : mLabels)
      p.second->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(p.second->GetParameterId()));
}

void VST3ParametersWindow::UpdateParameter(Steinberg::Vst::ParamID paramId)
{
   {
      auto it = mControls.find(paramId);
      if(it != mControls.end())
         it->second->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(it->second->GetParameterId()));
   }
   {
      auto it = mLabels.find(paramId);
      if(it != mLabels.end())
         it->second->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(it->second->GetParameterId()));
   }
}

void VST3ParametersWindow::RegisterParameterControl(VST3ParameterControl* control)
{
   mControls[control->GetParameterId()] = control;
   control->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(control->GetParameterId()));
}

void VST3ParametersWindow::RegisterParameterLabel(VST3ParameterControl* label)
{
   mLabels[label->GetParameterId()] = label;
   label->SetNormalizedValue(*mEditController, mEditController->getParamNormalized(label->GetParameterId()));
}

void VST3ParametersWindow::OnParameterValueChanged(const wxCommandEvent& evt)
{
   if(auto control = dynamic_cast<VST3ParameterControl*>(evt.GetEventObject()))
   {
      const auto paramId = control->GetParameterId();
      const auto normalizedValue = control->GetNormalizedValue(*mEditController);

      mEditController->setParamNormalized(paramId, normalizedValue);
      if(mComponentHandler->beginEdit(paramId) == Steinberg::kResultOk)
      {
         auto cleanup = finally([=] { mComponentHandler->endEdit(paramId); });
         mComponentHandler->performEdit(paramId, normalizedValue);
      }
      auto it = mLabels.find(paramId);
      if (it != mLabels.end()) {
         it->second->SetNormalizedValue(*mEditController, normalizedValue);
         control->UpdateAccessible(*mEditController, normalizedValue);
      }
   }
}

VST3ParametersWindow* VST3ParametersWindow::Setup(wxWindow& parent, Steinberg::Vst::IEditController& editController,
   Steinberg::Vst::IComponentHandler& componentHandler)
{
   constexpr int WindowBorder { 5 };
   constexpr int WindowMaxHeight { 450 };

   auto parametersWindow = safenew VST3ParametersWindow(&parent,
      editController,
      componentHandler,
      wxID_ANY,
      wxDefaultPosition,
      wxDefaultSize,
      wxVSCROLL | wxTAB_TRAVERSAL);
   // This fools NVDA into not saying "Panel" when the dialog gets focus
   parametersWindow->SetName(wxT("\a"));
   parametersWindow->SetLabel(wxT("\a"));

   auto mainSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
   
   mainSizer->Add(parametersWindow, 1, wxEXPAND | wxALL, WindowBorder);

   auto minSize = parametersWindow->GetSizer()->CalcMin();
   if(minSize.GetHeight() > (WindowMaxHeight - WindowBorder * 2))
   {
      minSize.SetHeight(WindowMaxHeight);
      parametersWindow->SetScrollRate(0, 20);
   }
   else
      minSize.y += WindowBorder * 2;
   
   parent.SetMinSize(minSize);
   parent.SetSizer(mainSizer.release());

   return parametersWindow;
}
