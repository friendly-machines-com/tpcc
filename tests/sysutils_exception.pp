program SysUtilsException;

uses
  SysUtils;

type
  EChild = class(Exception)
  end;

var
  E: Exception;
  Child: EChild;

begin
  E := Exception.Create('first');
  if E.Message <> 'first' then
    Halt(1);
  if E.HelpContext <> 0 then
    Halt(1);
  E.Message := 'changed';
  if E.Message <> 'changed' then
    Halt(1);
  E.Free;

  E := Exception.CreateHelp('help', 17);
  if E.Message <> 'help' then
    Halt(1);
  if E.HelpContext <> 17 then
    Halt(1);
  E.Free;

  // Constructors inherited from Exception allocate the selected descendant,
  // just like any other inherited Pascal constructor.
  Child := EChild.Create('child');
  if Child.Message <> 'child' then
    Halt(1);
  Child.Free
end.
