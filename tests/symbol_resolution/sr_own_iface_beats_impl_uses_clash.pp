unit sr_own_iface_beats_impl_uses_clash;

{ TARGET rule 2 subtle case, direct test: own interface declares `Colliding`;
  impl-uses sr_impl_const also declares `Colliding`. Inside the
  implementation, the own-interface value must win. }

interface

const
  Colliding = 44;

function Read: Integer;

implementation

uses sr_impl_const;      { sr_impl_const.Colliding = 20 }

function Read: Integer;
begin
  Result := Colliding    { TARGET: own interface = 44, NOT impl-uses = 20 }
end;

end.
