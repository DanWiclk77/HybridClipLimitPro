/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useRef } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { 
  Zap, 
  Settings2, 
  Activity, 
  Target, 
  Maximize2, 
  ChevronRight, 
  Layers,
  Wind,
  Volume2,
  Lock,
  Cpu
} from 'lucide-react';

// --- Types ---
type LimiterStyle = 'Transparent' | 'Punchy' | 'Dynamic' | 'Modern' | 'Aggressive';

// --- Components ---

const Knob = ({ label, value, min, max, onChange, unit = "" }: { 
  label: string, value: number, min: number, max: number, onChange: (v: number) => void, unit?: string 
}) => {
  return (
    <div className="flex flex-col items-center gap-2 group">
      <div className="relative w-16 h-16 flex items-center justify-center rounded-full bg-zinc-900 border-2 border-zinc-800 group-hover:border-blue-500 transition-colors shadow-[inset_0_2px_4px_rgba(0,0,0,0.5)]">
        <div 
          className="w-1 h-6 bg-blue-500 absolute top-1 origin-bottom rounded-full"
          style={{ transform: `rotate(${(value - min) / (max - min) * 270 - 135}deg)` }}
        />
        <span className="text-[10px] font-mono text-zinc-400 group-hover:text-white transition-colors">
          {value.toFixed(1)}{unit}
        </span>
      </div>
      <span className="text-[10px] uppercase tracking-wider text-zinc-500 font-bold group-hover:text-zinc-300">
        {label}
      </span>
    </div>
  );
};

const Meter = ({ value, label, color = "bg-blue-500" }: { value: number, label: string, color?: string }) => {
  // value is 0 to 100 for percentage visualization
  return (
    <div className="flex flex-col items-center h-full gap-2">
      <div className="w-4 h-48 bg-zinc-900 rounded-sm relative overflow-hidden border border-zinc-800">
        <motion.div 
          initial={{ height: 0 }}
          animate={{ height: `${value}%` }}
          className={`absolute bottom-0 w-full ${color} opacity-80 shadow-[0_0_10px_rgba(59,130,246,0.5)]`}
          transition={{ type: 'spring', stiffness: 300, damping: 30 }}
        />
        {/* dB scale marks */}
        {[0, -3, -6, -12, -18, -24, -48].map((db) => (
          <div 
            key={db}
            className="absolute right-0 w-1 h-[1px] bg-zinc-700"
            style={{ bottom: `${(db + 48) / 48 * 100}%` }}
          />
        ))}
      </div>
      <span className="text-[9px] uppercase tracking-tighter text-zinc-600 font-bold">{label}</span>
    </div>
  );
};

export default function App() {
  // Clipper State
  const [clipGain, setClipGain] = useState(0);
  const [clipKnee, setClipKnee] = useState(0.5);
  const [oversampling, setOversampling] = useState(1);
  
  // Limiter State
  const [limitThreshold, setLimitThreshold] = useState(0);
  const [limitStyle, setLimitStyle] = useState<LimiterStyle>('Modern');
  const [lookAhead, setLookAhead] = useState(1.0);
  const [truePeak, setTruePeak] = useState(true);

  // Auto-Target State
  const [isCalibrating, setIsCalibrating] = useState(false);
  const [targetLufs, setTargetLufs] = useState(-8);

  const startAutoCalibration = () => {
    setIsCalibrating(true);
    // Simulate AI Calibration
    setTimeout(() => {
      // Calculate settings for target
      const offset = Math.abs(targetLufs + 14); // basic heuristic
      setClipGain(offset * 0.4);
      setLimitThreshold(-(offset * 0.6));
      setIsCalibrating(false);
    }, 2000);
  };

  return (
    <div className="min-h-screen bg-[#0a0a0c] text-zinc-100 font-sans p-8 flex items-center justify-center selection:bg-blue-500/30">
      <div className="w-full max-w-5xl rounded-2xl bg-zinc-950 border border-zinc-800 shadow-2xl overflow-hidden flex flex-col md:flex-row h-[600px]">
        
        {/* --- Main Control Area --- */}
        <div className="flex-1 p-8 flex flex-col gap-8 relative overflow-hidden">
          
          {/* Background Ambient Glow */}
          <div className="absolute top-0 right-0 w-64 h-64 bg-blue-600/5 blur-[120px] pointer-events-none" />
          <div className="absolute bottom-0 left-0 w-64 h-64 bg-purple-600/5 blur-[120px] pointer-events-none" />

          {/* Header */}
          <header className="flex justify-between items-center z-10">
            <div className="flex items-center gap-3">
              <div className="p-2 bg-blue-600 rounded-lg shadow-lg shadow-blue-900/20">
                <Layers className="w-6 h-6 text-white" />
              </div>
              <div>
                <h1 className="text-xl font-bold tracking-tight bg-clip-text text-transparent bg-gradient-to-r from-white to-zinc-400">
                  LOUDNESS CATALYST
                </h1>
                <p className="text-[10px] uppercase tracking-[0.2em] text-blue-500 font-bold">
                  2-in-1 Analog Master Suite
                </p>
              </div>
            </div>

            <div className="flex items-center gap-4">
              <div className="hidden lg:flex items-center gap-1 px-3 py-1 bg-zinc-900 rounded-md border border-zinc-800">
                 <Wind className="w-3 h-3 text-cyan-400" />
                 <span className="text-[9px] uppercase font-bold text-zinc-500">True Peak On</span>
              </div>
              <button 
                onClick={startAutoCalibration}
                className={`flex items-center gap-2 px-4 py-2 rounded-full border transition-all ${isCalibrating ? 'border-blue-500 bg-blue-500/10' : 'border-zinc-800 hover:border-blue-500'}`}
              >
                {isCalibrating ? (
                  <motion.div 
                    animate={{ rotate: 360 }}
                    transition={{ repeat: Infinity, duration: 1, ease: 'linear' }}
                  >
                    <Settings2 className="w-4 h-4 text-blue-400" />
                  </motion.div>
                ) : (
                  <Target className="w-4 h-4 text-zinc-400" />
                )}
                <span className="text-xs font-bold uppercase tracking-wider text-white">
                  {isCalibrating ? "Matching LUFS..." : "Smart Target"}
                </span>
                {!isCalibrating && (
                  <select 
                    value={targetLufs} 
                    onChange={(e) => setTargetLufs(Number(e.target.value))}
                    className="bg-transparent border-none text-blue-400 focus:ring-0 cursor-pointer pointer-events-auto ml-1 font-mono text-xs"
                    onClick={(e) => e.stopPropagation()}
                  >
                    <option value={-6}>-6.0</option>
                    <option value={-8}>-8.0</option>
                    <option value={-10}>-10.0</option>
                    <option value={-14}>-14.0</option>
                  </select>
                )}
              </button>
            </div>
          </header>

          {/* Signal Chain Visualization */}
          <div className="flex flex-1 gap-6 z-10">
            
            {/* Stage 1: CLIPPER */}
            <section className="flex-1 bg-zinc-900/40 rounded-xl border border-zinc-800/50 p-6 flex flex-col gap-6 backdrop-blur-sm">
              <div className="flex items-center gap-2 border-b border-zinc-800 pb-3">
                <Zap className="w-4 h-4 text-amber-400" />
                <h2 className="text-xs font-black uppercase tracking-widest text-zinc-400">Master Clipper</h2>
              </div>
              
              <div className="flex justify-around items-end flex-1 pb-4">
                <Knob label="Input Gain" value={clipGain} min={0} max={24} unit="dB" onChange={setClipGain} />
                <Knob label="Knee" value={clipKnee} min={0} max={1} onChange={setClipKnee} />
                <Knob label="Ceiling" value={0} min={-6} max={0} unit="dB" onChange={() => {}} />
              </div>

              <div className="grid grid-cols-2 gap-2 text-[9px] uppercase font-bold">
                <button 
                  onClick={() => setOversampling(oversampling === 1 ? 4 : 1)}
                  className={`p-2 rounded border transition-colors ${oversampling > 1 ? 'border-amber-500 text-amber-400 bg-amber-500/5' : 'border-zinc-800 text-zinc-500 hover:border-zinc-700'}`}
                >
                  Oversampling x{oversampling}
                </button>
                <button className="p-2 rounded border border-zinc-800 text-zinc-500 hover:border-zinc-700 transition-colors uppercase">
                  Soft / Hard
                </button>
              </div>
            </section>

            {/* Stage 2: LIMITER */}
            <section className="flex-1 bg-zinc-900/40 rounded-xl border border-zinc-800/50 p-6 flex flex-col gap-6 backdrop-blur-sm">
              <div className="flex items-center gap-2 border-b border-zinc-800 pb-3">
                <Volume2 className="w-4 h-4 text-blue-400" />
                <h2 className="text-xs font-black uppercase tracking-widest text-zinc-400">Master Limiter</h2>
              </div>

              <div className="flex justify-around items-end flex-1 pb-4">
                <Knob label="Gain" value={Math.abs(limitThreshold)} min={0} max={24} unit="dB" onChange={(v) => setLimitThreshold(-v)} />
                <Knob label="Look-ahead" value={lookAhead} min={0} max={5} unit="ms" onChange={setLookAhead} />
                <Knob label="Release" value={200} min={1} max={1000} unit="ms" onChange={() => {}} />
              </div>

              <div className="flex flex-col gap-2">
                 <div className="flex bg-zinc-950 border border-zinc-800 p-1 rounded-lg">
                    {['Dynamic', 'Modern', 'Transparent'].map((style) => (
                      <button
                        key={style}
                        onClick={() => setLimitStyle(style as LimiterStyle)}
                        className={`flex-1 text-[9px] font-bold py-1 px-2 rounded-md transition-all ${limitStyle === style ? 'bg-blue-600 text-white' : 'text-zinc-600 hover:text-zinc-400 hover:bg-zinc-900'}`}
                      >
                        {style}
                      </button>
                    ))}
                 </div>
                 <div className="flex items-center justify-between px-2">
                    <span className="text-[9px] uppercase font-bold text-zinc-600">True Peak</span>
                    <button 
                      onClick={() => setTruePeak(!truePeak)}
                      className={`w-8 h-4 rounded-full relative transition-colors ${truePeak ? 'bg-blue-600' : 'bg-zinc-800'}`}
                    >
                      <motion.div 
                        animate={{ left: truePeak ? '18px' : '2px' }}
                        className="absolute top-[2px] w-3 h-3 bg-white rounded-full shadow-sm"
                      />
                    </button>
                 </div>
              </div>
            </section>

          </div>

          {/* Footer Status Bar */}
          <footer className="flex items-center justify-between z-10 border-t border-zinc-800/50 pt-4">
            <div className="flex gap-4">
              <div className="flex items-center gap-1">
                <span className="w-2 h-2 rounded-full bg-green-500 shadow-[0_0_5px_rgba(34,197,94,0.5)]" />
                <span className="text-[10px] font-mono text-zinc-500 uppercase tracking-tighter">Engine Ready</span>
              </div>
              <div className="flex items-center gap-1">
                <Cpu className="w-3 h-3 text-zinc-600" />
                <span className="text-[10px] font-mono text-zinc-500 uppercase tracking-tighter">0.8% Load</span>
              </div>
            </div>
            <div className="flex items-center gap-2">
               <span className="text-[10px] font-mono text-zinc-500 lowercase">v.1.0.0-beta</span>
               <Lock className="w-3 h-3 text-zinc-700" />
            </div>
          </footer>

        </div>

        {/* --- Metering Sidebar --- */}
        <div className="w-full md:w-56 bg-black p-8 border-l border-zinc-800 flex flex-col gap-6">
           <div className="flex items-center gap-2 border-b border-zinc-800/50 pb-3">
              <Activity className="w-4 h-4 text-zinc-400" />
              <h2 className="text-[10px] font-black uppercase tracking-widest text-zinc-600">Metering</h2>
           </div>

           <div className="flex-1 flex justify-between gap-4">
              <Meter value={75 + clipGain} label="In" />
              <Meter value={40 + (clipGain * 1.5)} label="GR" color="bg-red-500" />
              <Meter value={targetLufs < -10 ? 60 : 85} label="LUFS" color="bg-green-500" />
              <Meter value={80} label="TP" color="bg-cyan-500" />
              <Meter value={90} label="Out" />
           </div>

           <div className="flex flex-col gap-2 pt-4 border-t border-zinc-800/50 text-center">
              <div className="flex justify-between items-baseline">
                <span className="text-[10px] text-zinc-500 uppercase font-bold tracking-wide">Integrated</span>
                <span className="text-sm font-mono text-zinc-300">{-7.8} LUFS</span>
              </div>
              <div className="flex justify-between items-baseline">
                <span className="text-[10px] text-zinc-500 uppercase font-bold tracking-wide">Short-Term</span>
                <span className="text-sm font-mono text-zinc-400">{-6.2} LUFS</span>
              </div>
              <div className="mt-4 flex items-center justify-center p-2 rounded-lg bg-zinc-900 border border-zinc-800 hover:border-blue-500 cursor-help transition-all">
                <Maximize2 className="w-4 h-4 text-zinc-500" />
              </div>
           </div>
        </div>

      </div>
    </div>
  );
}
