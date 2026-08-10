# Helicopter insertion / extraction Blueprint setup

The C++ layer owns authority, replication, timing, passenger attachment, landing-zone checks, rappel movement, boarding, and extraction settlement. Blueprint children only need to provide presentation assets and authored paths.

## 1. Helicopter Blueprint

Create `BP_OBInsertionHelicopter` as a child of `AOBInsertionHelicopter`.

The supplied Vigilante `BP_West_Heli_UH60A` is an `Actor`, so it cannot be assigned directly to the GameMode class property. Duplicate/reparent it to `AOBInsertionHelicopter`, or create a new child and copy its visual components beneath `VisualRoot`. Do not modify the vendor original.

Position these inherited components in the child Blueprint:

- `CabinCamera`
- `Seat_00` through `Seat_11`
- `Rappel_Left`
- `Rappel_Right`
- `GroundDust`

Assign these direct uasset parameters in Class Defaults:

- `Rotor Loop Sound`
- `Ground Dust System`

Implement presentation events as needed:

- `On Mission Changed`
- `On Insertion Phase Changed`
- `On Extraction Phase Changed`
- `On Doors Changed`
- `On Rappel Line Changed`
- `On Passenger Seated`
- `On Passenger Rappel Started`
- `On Passenger Landed`

The existing helicopter functions such as rotor speed and door controls should be called from these events. A Cable Component or Niagara rope should be visual only; character descent is already moved authoritatively by C++.

Set this Blueprint on `BP_ExpeditionGameMode`:

- `Insertion Helicopter Class`
- `Extraction Helicopter Class` (optional; falls back to the insertion class)

## 2. Signal flare Blueprint

Create `BP_OBSignalFlare` as a child of `AOBSignalFlare` and assign:

- `Trail System`
- `Burst System`
- `Persistent Smoke System`
- `Launch Sound`
- `Burst Sound`

Optional events `On Flare Launched` and `On Flare Burst` can drive a mesh, light, camera shake, or additional Niagara components. Assign this class to each extraction-zone Blueprint's `Signal Flare Class`.

## 3. Insertion routes

Place `AOBHelicopterRoute` actors in the expedition map.

- Set `Purpose = Insertion Orbit`.
- Enable `Loop`.
- Shape `FlightPath` as a closed orbit.
- Use `Team Slot = 0` for any team, or a non-zero slot to reserve a route.

Provide at least as many routes as simultaneous teams. If no route is available, C++ uses a procedural circle around `WorldMapCenter`.

Route actors, extraction zones, and extraction-site definitions should be Always Loaded / not spatially loaded in World Partition. The helicopter itself contains a streaming-source component and loads cells while moving.

## 4. World map insertion selection

On the `UOBWorldMapWidget` Blueprint child, assign `Insertion Target Icon` and its color. Native code now distinguishes a click from a drag and sends the clicked world XY to the server. Only the party leader can commit a point.

The server validates map bounds and scans nearby terrain. Tune the inherited `LandingZoneScanner` component on `BP_ExpeditionGameMode`:

- search radius and ring step
- maximum slope
- footprint height variance
- navigation requirement
- hover height
- helicopter clearance capsule

Place `AOBHelicopterExclusionVolume` over water, interiors, roofs, or other invalid insertion areas.

## 5. Extraction zones

Existing `BP_ExtractionZone_Public` and `BP_ExtractionZone_Personal` remain children of `AOBExtractionZone`, but their behavior changed:

- entering the call trigger launches one flare;
- `Helicopter Call Delay` controls arrival time;
- `Inbound Lead Time` controls when the helicopter becomes visible and starts approach;
- `Boarding Window Seconds` controls how long doors remain open;
- the existing `Hold Time` now means door-boarding hold time, not instant extraction time;
- loot is settled only after helicopter departure.

Move the inherited `LandingAnchor`, `FlareLaunchAnchor`, and `BoardingTrigger` to the desired positions. Assign optional `Approach Route`, `Exit Route`, `Signal Flare Class`, and an extraction-specific helicopter class.

Useful presentation events:

- `On Extraction Active Changed`
- `On Call Phase Changed`
- `On Flare Launched`
- `On Helicopter Spawned`
- `On Passenger Boarded`

HUD widgets can read `Get Call State`, `Get Arrival Seconds Remaining`, and `Get Boarding Seconds Remaining`. Countdown values are based on replicated server timestamps.

## 6. Authored extraction sites

For personal extraction candidates, replace a tagged `TargetPoint` with `AOBExtractionSite` when a custom flight path is required. The actor automatically has the `PersonalExtract` tag.

Configure:

- `LandingAnchor`
- `FlareAnchor`
- `ApproachRoute` with `Purpose = Extraction Approach`
- `ExitRoute` with `Purpose = Extraction Exit`

Legacy tagged TargetPoints still work and use dynamic straight-line approach/departure offsets.

## 7. Character presentation

C++ disables movement/collision, attaches the real Pawn to a seat, locks gameplay input, cancels abilities, and applies `State.HelicopterTransit`. Use these helicopter events to play seat/rappel montages. On landing, C++ restores walking, collision, view target, input, and removes the transit tag.

## 8. Recommended PIE validation

1. Dedicated server with 1, 2, and 4 clients.
2. Party leader and non-leader map clicks.
3. Water, steep terrain, building, and valid-flat-ground selections.
4. Join while orbiting and join after rappel starts.
5. Personal and public extraction calls.
6. Leave the extraction trigger during ETA, then return during boarding.
7. Miss the boarding window.
8. Disconnect during insertion, waiting, boarding, and departure.
9. Session timeout during an active extraction.
10. World Partition visualization along orbit, approach, landing, and exit paths.
