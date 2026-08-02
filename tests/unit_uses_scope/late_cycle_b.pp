unit Late_Cycle_B;

interface

function SettingIsDisabled: Boolean;

implementation

uses Late_Cycle_A;

function SettingIsDisabled: Boolean;
begin
  Result := Late_Cycle_A.CurrentSettings.Disabled
end;

end.
