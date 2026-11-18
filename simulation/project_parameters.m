function p = project_parameters
% Editable plant assumptions. L and loss resistances require hardware confirmation.
p.L=100e-6;
p.C_low=440e-6; % C18+C19, 220uF each, confirmed in supplied EasyEDA source.
p.C_high=220e-6; % Example bus capacitor; confirm against assembled hardware.
p.inductor_resistance=0.10; % Lumped copper/conduction loss, assumed ohm.
p.source_resistance=0.15; % Boost supply/lead impedance, assumed ohm.
p.load_ohm=5;
p.buck_input_V=12;
p.boost_input_V=3;
p.duration=40;
p.start_s=0.2;
p.mode_change_s=12;
p.open_percent=20;
p.adc_quantization=true;
p.adc_zero=2076.36098063;
p.Ts=50e-6;
p.arr=3750;
% Gains are read from firmware so simulation does not silently drift from code.
header=fileread(fullfile(fileparts(mfilename('fullpath')),'..','ccs_project','include','control_settings.h'));
pairs={'buck_current_kp','BUCK_CURRENT_KP';'buck_current_ki','BUCK_CURRENT_KI'; ...
 'boost_current_kp','BOOST_CURRENT_KP';'boost_current_ki','BOOST_CURRENT_KI'; ...
 'buck_dual_current_kp','BUCK_DUAL_CURRENT_KP';'buck_dual_current_ki','BUCK_DUAL_CURRENT_KI'; ...
 'boost_dual_current_kp','BOOST_DUAL_CURRENT_KP';'boost_dual_current_ki','BOOST_DUAL_CURRENT_KI'; ...
 'buck_voltage_kp','BUCK_VOLTAGE_KP';'buck_voltage_ki','BUCK_VOLTAGE_KI'; ...
 'boost_voltage_kp','BOOST_VOLTAGE_KP';'boost_voltage_ki','BOOST_VOLTAGE_KI'; ...
 'buck_single_kp','BUCK_SINGLE_VOLTAGE_KP';'buck_single_ki','BUCK_SINGLE_VOLTAGE_KI'; ...
 'buck_dual_limit','BUCK_DUAL_CURRENT_MAX_A';'boost_dual_limit','BOOST_DUAL_CURRENT_MAX_A'};
for k=1:size(pairs,1)
    match=regexp(header,['#define\s+' pairs{k,2} '\s+([0-9.eE+-]+)'],'tokens','once');
    assert(~isempty(match),'Missing firmware parameter: %s',pairs{k,2});
    p.(pairs{k,1})=str2double(match{1});
end
end
