program LateCycleMain;

uses Late_Cycle_A, Late_Cycle_B;

begin
  CurrentSettings.Disabled := True;
  if not SettingIsDisabled then
    Halt(1)
end.
