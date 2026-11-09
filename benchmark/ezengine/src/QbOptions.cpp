#include <Foundation/Utilities/CommandLineUtils.h>

#include <QbOptions.h>
#include <cstdlib>

void QbParseOptions(QbOptions& out_opt)
{
  const ezDynamicArray<ezString>& args = ezCommandLineUtils::GetGlobalInstance()->GetCommandLineArray();
  for (ezUInt32 i = 0; i < args.GetCount(); ++i)
  {
    const std::string a = args[i].GetData();
    if (i > 0)
    {
      if (!out_opt.m_sCmdline.empty())
        out_opt.m_sCmdline += ' ';
      out_opt.m_sCmdline += a;
    }
    if (a.compare(0, 2, "--") != 0)
      continue;
    const size_t eq = a.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string k = a.substr(2, eq - 2);
    const std::string v = a.substr(eq + 1);
    if (k == "scenario")
      out_opt.m_sScenario = v;
    else if (k == "mode")
      out_opt.m_sMode = v;
    else if (k == "run")
      out_opt.m_sRun = v;
    else if (k == "out")
      out_opt.m_sOut = v;
    else if (k == "warmup")
    {
      out_opt.m_fWarmup = atof(v.c_str());
      out_opt.m_bWarmupGiven = true;
    }
    else if (k == "measure")
      out_opt.m_fMeasure = atof(v.c_str());
    else if (k == "flavour")
      out_opt.m_sFlavour = v;
    else if (k == "camera")
      out_opt.m_sCamera = v;
    else if (k == "sleeping")
      out_opt.m_sSleeping = v;
    else if (k == "sleep-vel")
      out_opt.m_fSleepVel = (float)atof(v.c_str());
    else if (k == "sleep-time")
      out_opt.m_fSleepTime = (float)atof(v.c_str());
    else if (k == "profile")
      out_opt.m_sProfile = v;
    else if (k == "shadows")
      out_opt.m_sShadows = v;
    else if (k == "shadow-map")
      out_opt.m_iShadowMap = atoi(v.c_str());
    else if (k == "msaa")
      out_opt.m_iMsaa = atoi(v.c_str());
    else if (k == "vsync")
      out_opt.m_sVsync = v;
    else if (k == "bodies")
      out_opt.m_iBodies = atoi(v.c_str());
    else if (k == "rate")
      out_opt.m_fRate = atof(v.c_str());
    else if (k == "unpaced")
      out_opt.m_bUnpaced = (v == "1" || v == "true");
    else if (k == "ray-api")
      out_opt.m_sRayApi = v;
    else if (k == "mt-render")
      out_opt.m_bMtRender = (v == "1" || v == "true" || v == "on");
    else if (k == "max-bodies")
      out_opt.m_iMaxBodies = atoi(v.c_str());
    else if (k == "sun-intensity")
      out_opt.m_fSunIntensity = atof(v.c_str());
    else if (k == "ambient-intensity")
      out_opt.m_fAmbientIntensity = atof(v.c_str());
    else if (k == "background")
      out_opt.m_sBackground = v;
    else if (k == "slice-ms")
      out_opt.m_fSliceMs = atof(v.c_str());
    else if (k == "project")
      out_opt.m_sProjectOverride = v;
  }
}
