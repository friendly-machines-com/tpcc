unit sr_own_impl_beats_impl_uses_clash;

{ TARGET rule 2: own implementation decl wins over impl-uses decl of the
  same name. }

interface

function Read: Integer;

implementation

uses sr_impl_const;      { sr_impl_const.Colliding = 20 }

const
  Colliding = 77;        { own implementation decl }

function Read: Integer;
begin
  Result := Colliding    { TARGET: own impl = 77, NOT impl-uses = 20 }
end;

end.
