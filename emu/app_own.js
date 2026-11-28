// SPDX-License-Identifier: MPL-2.0
// Ear Olympics' part of the emulator page: the keyboard shortcuts and the MIDI keyboard, listed under the controls.
export function attach() {
  const bar = document.querySelector(".bar");
  if (!bar) return;
  const d = document.createElement("details");
  d.className = "speak";
  d.open = true;
  d.innerHTML = `
    <style>.speak table { border-collapse: collapse; margin: .4rem 0; } .speak th { text-align: left; padding: .15rem 1.2rem .15rem 0; white-space: nowrap; vertical-align: top; font-weight: 600; } .speak td { padding: .15rem 0; }</style>
    <summary><h2>Keyboard shortcuts</h2></summary>
    <p>Click the panel once so it has focus, then use these. They can be turned off with the checkbox above.</p>
    <table>
      <tbody>
        <tr><th scope="row">P</th><td>PLAY: start, and hear the melody again during your turn</td></tr>
        <tr><th scope="row">Esc</th><td>HOME: stop, or skip the cat's talk</td></tr>
        <tr><th scope="row">Up / Down arrow</th><td>The SELECT knob: choose the root, or step through the melody one note at a time</td></tr>
        <tr><th scope="row">Q W E R T Y U I</th><td>The first eight white keys</td></tr>
        <tr><th scope="row">A S D F G H J K</th><td>The next eight white keys</td></tr>
        <tr><th scope="row">1 2 3 4 5 6 7 8 9 0 -</th><td>The black keys</td></tr>
        <tr><th scope="row">Z / X</th><td>OCT- / OCT+ (the game doesn't use them)</td></tr>
        <tr><th scope="row">Tab, then arrows</th><td>Move to any knob or button, then turn it with the arrow keys (Page Up and Page Down are faster) or press it with Space</td></tr>
      </tbody>
    </table>
    <p class="note">The other buttons and knobs (SEL, SAVE, PRESETS, KNOB 1 to 4) can be clicked, or reached with Tab.</p>
    <h2 style="margin-top:1rem">MIDI keyboard</h2>
    <p class="note">Press "MIDI ports" above, allow MIDI, and choose your keyboard as the input. MIDI notes 53 to 79 (F3 to G5) play the 27 keys. Other notes are ignored.</p>`;
  bar.insertAdjacentElement("afterend", d);
}
