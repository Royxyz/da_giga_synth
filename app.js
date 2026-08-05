function synthApp() {
    return {
        ws: null,
        connected: false,
        
        telemetry: {
            cutoff: 2000, res: 0, morph1: 0, morph2: 0, 
            lfo1Rate: 0, abyssSend: 0, oscMix: 0, currentStep: 0,
            osc1Bank: 0, osc2Bank: 0, filterMode: 0, fxFreeze: false
        },
        
        matrix: Array(30).fill(0.0),

        envs: {
            amp:  { A: 10, D: 100, S: 0.8, R: 200 },
            mod1: { A: 10, D: 100, S: 0.0, R: 100 },
            mod2: { A: 10, D: 100, S: 0.0, R: 100 }
        },

        lfo1Rate: 1.0,
        lfo2Rate: 2.0,

        scaleNotes: Array.from({length: 61}, (_, i) => 84 - i), 
        activeNotesList: [], 
        noteIdCounter: 0,
        
        bpm: 120,
        numSteps: 16,     
        useFlats: true, 

        activeTool: 'draw',
        dragAction: null, 
        currentNoteId: null,
        dragStartPos: { step: 0, noteIndex: 0 },
        originalNoteState: null,

        uploadBank: "0",
        isUploading: false,
        uploadStatus: "STATUS: AWAITING_INPUT",

        init() {
            this.connectWS();
            const savedSeq = localStorage.getItem('synth_sequence');
            if (savedSeq) this.activeNotesList = JSON.parse(savedSeq);
            const savedSteps = localStorage.getItem('synth_steps');
            if (savedSteps) this.numSteps = parseInt(savedSteps, 10);
            const savedBpm = localStorage.getItem('synth_bpm');
            if (savedBpm) this.bpm = parseFloat(savedBpm);
        },

        connectWS() {
            const wsUrl = `ws://${window.location.hostname}:81/`;
            this.ws = new WebSocket(wsUrl);
            this.ws.onopen = () => { this.connected = true; };
            this.ws.onclose = () => { 
                this.connected = false; 
                setTimeout(() => this.connectWS(), 2000); 
            };
            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === "telemetry") {
                        this.telemetry = { ...this.telemetry, ...data };
                    }
                } catch (e) { }
            };
        },

        updateEnv(target, stage, val) {
            if (this.connected) this.ws.send(JSON.stringify({ type: "env", target: target, stage: stage, val: parseFloat(val) }));
        },

        updateMatrix(src, dest, val) {
            const floatVal = parseFloat(val) || 0.0;
            this.matrix[(src * 5) + dest] = floatVal;
            if (this.connected) this.ws.send(JSON.stringify({ type: "matrix", src: src, dest: dest, val: floatVal }));
        },

        updateBPM() {
            if (this.connected) this.ws.send(JSON.stringify({ type: "bpm", val: this.bpm }));
        },

        updateLFORate(id, val) {
            if (this.connected) this.ws.send(JSON.stringify({ type: "lfoRate", id: id, val: parseFloat(val) }));
        },

        noteName(midi) {
            const sharps = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
            const flats = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];
            const notes = this.useFlats ? flats : sharps;
            const octave = Math.floor(midi / 12) - 1;
            return `${notes[midi % 12]}${octave}`;
        },

        isBlackKey(midi) {
            return [1, 3, 6, 8, 10].includes(midi % 12);
        },

        getGridCoords(e) {
            const rect = this.$refs.dawGrid.getBoundingClientRect();
            return {
                x: e.clientX - rect.left,
                y: e.clientY - rect.top
            };
        },

        gridPointerDown(e) {
            e.preventDefault(); // Strongly prevent mobile swipe-to-scroll interruptions[cite: 3]

            if (e.button === 2 || this.activeTool === 'erase') { 
                this.dragAction = 'erase';
                this.processErase(e);
                // FIXED: Must capture the pointer even when erasing an empty cell to allow drag-erasing[cite: 3]
                if (e.pointerId) this.$refs.dawGrid.setPointerCapture(e.pointerId);
                return;
            }
            if (e.button !== 0) return; 
            
            const coords = this.getGridCoords(e);
            const step = Math.floor(coords.x / 40);
            const noteIndex = Math.floor(coords.y / 20);
            
            if (step >= this.numSteps || step < 0 || step > 63 || noteIndex < 0 || noteIndex >= this.scaleNotes.length) return;

            const newNote = {
                id: ++this.noteIdCounter,
                step: step,
                midi: this.scaleNotes[noteIndex],
                length: 1
            };
            
            this.activeNotesList.push(newNote);
            this.dragAction = 'draw';
            this.currentNoteId = newNote.id;
            
            // FIXED: Anchor pointer capture to the stable parent grid, not e.target[cite: 3]
            if (e.pointerId) this.$refs.dawGrid.setPointerCapture(e.pointerId);
        },

        gridPointerMove(e) {
            if (!this.dragAction) return;
            
            if (this.dragAction === 'erase') {
                this.processErase(e);
                return;
            }
            
            const coords = this.getGridCoords(e);
            const currentStepHover = Math.floor(coords.x / 40);
            const currentNoteHover = Math.floor(coords.y / 20);
            
            const note = this.activeNotesList.find(n => n.id === this.currentNoteId);
            if (!note) return;
            
            if (this.dragAction === 'draw' || this.dragAction === 'resize') {
                let newLength = currentStepHover - note.step + 1;
                if (newLength < 1) newLength = 1;
                if (note.step + newLength > 64) newLength = 64 - note.step; 
                if (note.step + newLength > this.numSteps) newLength = this.numSteps - note.step;
                if (note.length !== newLength) note.length = newLength; 
            }
            else if (this.dragAction === 'move') {
                const stepDelta = currentStepHover - this.dragStartPos.step;
                const noteDelta = currentNoteHover - this.dragStartPos.noteIndex;
                
                let newStep = this.originalNoteState.step + stepDelta;
                let newIndex = this.scaleNotes.indexOf(this.originalNoteState.midi) + noteDelta;
                
                if (newStep < 0) newStep = 0;
                if (newStep + note.length > this.numSteps) newStep = this.numSteps - note.length;
                if (newIndex < 0) newIndex = 0;
                if (newIndex >= this.scaleNotes.length) newIndex = this.scaleNotes.length - 1;
                
                const newMidi = this.scaleNotes[newIndex];
                if (note.step !== newStep || note.midi !== newMidi) {
                    note.step = newStep;
                    note.midi = newMidi;
                }
            }
        },

        gridPointerUp(e) {
            this.dragAction = null;
            this.currentNoteId = null;
            try {
                if (e.pointerId) this.$refs.dawGrid.releasePointerCapture(e.pointerId);
            } catch(err) { }
        },

        processErase(e) {
            const coords = this.getGridCoords(e);
            const currentStepHover = Math.floor(coords.x / 40);
            const currentNoteHover = Math.floor(coords.y / 20);
            
            if (currentNoteHover >= 0 && currentNoteHover < this.scaleNotes.length) {
                const midi = this.scaleNotes[currentNoteHover];
                this.activeNotesList = this.activeNotesList.filter(n => 
                    !(n.midi === midi && currentStepHover >= n.step && currentStepHover < n.step + n.length)
                );
            }
        },

        notePointerDown(note, e) {
            e.preventDefault();

            if (e.button === 2 || this.activeTool === 'erase') { 
                this.activeNotesList = this.activeNotesList.filter(n => n.id !== note.id);
                this.dragAction = 'erase'; 
                if (e.pointerId) this.$refs.dawGrid.setPointerCapture(e.pointerId);
                return;
            }
            if (e.button === 0) {
                this.dragAction = 'move';
                this.currentNoteId = note.id;
                this.originalNoteState = { ...note };
                
                const coords = this.getGridCoords(e);
                this.dragStartPos = {
                    step: Math.floor(coords.x / 40),
                    noteIndex: Math.floor(coords.y / 20)
                };
                if (e.pointerId) this.$refs.dawGrid.setPointerCapture(e.pointerId);
            }
        },

        resizePointerDown(note, e) {
            e.preventDefault();
            
            if (e.button !== 0 || this.activeTool === 'erase') return; 
            this.dragAction = 'resize';
            this.currentNoteId = note.id;
            
            // FIXED: Anchor pointer capture to the stable parent grid[cite: 3]
            // This stops mobile browsers from dropping the touch event when the note width resizes
            if (e.pointerId) this.$refs.dawGrid.setPointerCapture(e.pointerId);
        },
        
        pushSequence() {
            const stepsArr = [];
            for (let s = 0; s < this.numSteps; s++) {
                const stepEvents = [];
                const notesOnStep = this.activeNotesList.filter(n => n.step === s);
                for (let note of notesOnStep) {
                    stepEvents.push({ n: note.midi, l: note.length }); 
                    if (stepEvents.length >= 6) break; 
                }
                stepsArr.push(stepEvents);
            }

            if (this.connected) {
                this.ws.send(JSON.stringify({ type: "sequence", numSteps: this.numSteps, steps: stepsArr }));
                localStorage.setItem('synth_sequence', JSON.stringify(this.activeNotesList));
                localStorage.setItem('synth_steps', this.numSteps.toString());
                localStorage.setItem('synth_bpm', this.bpm.toString());
            }
        },

        async uploadWavetable() {
            const fileInput = this.$refs.fileInput;
            if (!fileInput.files.length) { this.uploadStatus = "ERR: NO PAYLOAD SELECTED"; return; }
            const file = fileInput.files[0];
            if (file.size !== 1048576) { this.uploadStatus = `ERR: SIZE MISMATCH. REQ: 1,048,576 BYTES.`; return; }

            this.isUploading = true;
            this.uploadStatus = "STATUS: UPLOADING TO NVRAM...";
            const formData = new FormData();
            const filename = "/" + file.name.toUpperCase().replace(/ /g, '_'); 
            formData.append("file", file, filename);

            try {
                const uploadRes = await fetch(`http://${window.location.hostname}/upload`, { method: 'POST', body: formData });
                if (uploadRes.ok) {
                    this.uploadStatus = "STATUS: UPLOAD VERIFIED. EXECUTING HOT-SWAP...";
                    const loadRes = await fetch(`http://${window.location.hostname}/load?file=${filename}&bank=${this.uploadBank}`);
                    if (loadRes.ok) { this.uploadStatus = `SYS: [${file.name}] LIVE IN BANK_${this.uploadBank}.`; } 
                    else { this.uploadStatus = "WARN: UPLOAD OK, HOT-SWAP FAILED."; }
                } else { this.uploadStatus = "ERR: UPLOAD FAILED (HTTP)."; }
            } catch (error) { this.uploadStatus = "ERR: CONNECTION SEVERED DURING UPLOAD.";
            } finally { this.isUploading = false; fileInput.value = ""; }
        }
    };
}