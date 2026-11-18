function run_project_simulation
root = fileparts(mfilename('fullpath'));
out = fullfile(root,'results');
if ~isfolder(out), mkdir(out); end
dataOut=fullfile(out,'data');
if ~isfolder(dataOut), mkdir(dataOut); end
p = project_parameters;
cases = {'buck_current','buck_dual','buck_open','buck_voltage', ...
         'boost_current','boost_dual','boost_open','buck_mode_change','boost_mode_change'};
summary = cell(numel(cases),8);
for k = 1:numel(cases)
    name = cases{k};
    fprintf('Simulating %s (%d/%d)\n',name,k,numel(cases));
    [t,y,meta] = simulate_case(name,p);
    tail = t > t(end)-1;
    summary(k,:) = {name,mean(y(tail,1)),mean(y(tail,2)), ...
        mean(y(tail,3)),mean(y(tail,4)),meta.peak_inductor_A,all(isfinite(y),'all'),NaN};
    if ~contains(name,'open')
        if contains(name,'current')
            if startsWith(name,'buck'), target=0.8; else, target=1.2; end
            error=abs(mean(y(tail,2))-target); tolerance=0.02;
        else
            if startsWith(name,'buck'), target=6; else, target=4.5; end
            error=abs(mean(y(tail,1))-target); tolerance=0.05;
        end
        summary{k,8}=error;
        assert(error<tolerance,'Steady error exceeds example tolerance: %s',name);
    end
    assert(all(isfinite(y),'all'),'Non-finite simulation: %s',name);
    assert(all(y(:,4)>=0 & y(:,4)<=0.9501),'Invalid duty: %s',name);
    plot_project_result(t,y,name,out);
    data = array2table([t,y],'VariableNames',{'time_s','output_V','port_current_A', ...
        'inductor_current_A','duty','enabled','control_mode','current_reference_A','voltage_target_V'});
    writetable(data(1:10:end,:),fullfile(dataOut,[name '.csv']));
    save(fullfile(dataOut,[name '.mat']),'t','y','p','meta');
end
metrics = cell2table(summary,'VariableNames',{'case_name','final_output_V', ...
    'final_port_current_A','final_inductor_A','final_duty','peak_inductor_A','finite','steady_abs_error_A_or_V'});
writetable(metrics,fullfile(out,'summary.csv'));
save(fullfile(dataOut,'parameters.mat'),'p');
disp(metrics);
fprintf('Results: %s\n',out);
end

function [t,y,meta] = simulate_case(name,p)
boost = startsWith(name,'boost');
if contains(name,'current'), mode=0;
elseif contains(name,'open'), mode=2;
elseif contains(name,'voltage'), mode=3;
else, mode=1;
end
if contains(name,'mode_change'), mode=0; end
N = round(p.duration/p.Ts);
stride = 20;
t = zeros(ceil(N/stride),1); y=zeros(numel(t),8); logged=0;
if boost
    x=[0;p.boost_input_V;p.boost_input_V];
else
    x=[0;0;0];
end
currentPI=[0;0]; voltagePI=[0;0];
lastMode=mode; count=0; actual=mode; command=0; peakCurrent=0;
for n=1:N
    time=(n-1)*p.Ts;
    requested=mode;
    if contains(name,'mode_change') && time>=p.mode_change_s, requested=1; end
    changed=requested~=lastMode;
    if changed
        actual=255; count=0; command=0;
        currentPI(:)=0; voltagePI(:)=0; lastMode=requested;
    end
    if actual==255
        count=count+1;
        if count>=200, actual=requested; end
    end
    gate = time>=p.start_s && actual~=255;
    if contains(name,'mode_change') && time>=p.mode_change_s && time<p.mode_change_s+0.025
        gate=false; % User reapplies a target AFTER mode acknowledgement.
    end
    if boost
        feedback=(p.boost_input_V-x(2))/p.source_resistance;
        output=x(3); vtarget=4.5; itarget=1.2;
        kp=p.boost_current_kp; ki=p.boost_current_ki;
        outerkp=p.boost_voltage_kp; outerki=p.boost_voltage_ki;
        maxI=p.boost_dual_limit; low=0.05; high=0.60;
    else
        feedback=x(2)/p.load_ohm;
        output=x(2); vtarget=6; itarget=0.8;
        kp=p.buck_current_kp; ki=p.buck_current_ki;
        outerkp=p.buck_voltage_kp; outerki=p.buck_voltage_ki;
        maxI=p.buck_dual_limit; low=0; high=0.95;
    end
    if p.adc_quantization
        direction=1-2*boost;
        raw=round(p.adc_zero+direction*feedback/(2*3.3/4095));
        feedback=direction*(raw-p.adc_zero)*(2*3.3/4095);
        rawV=round(output/15*4095/3.3);
        output=rawV*15*3.3/4095;
    end
    activeReference=0; targetVoltage=0;
    if actual==1
        if boost, kp=p.boost_dual_current_kp;ki=p.boost_dual_current_ki;
        else, kp=p.buck_dual_current_kp;ki=p.buck_dual_current_ki;
        end
    end
    if ~gate
        currentPI(:)=0; voltagePI(:)=0; command=0;
    elseif actual==2
        command=p.open_percent/100;
    elseif actual==3
        targetVoltage=vtarget;
        [command,currentPI]=pi_step(vtarget-output,p.buck_single_kp/100, ...
            p.buck_single_ki/100,p.Ts,currentPI,low,high);
    else
        activeReference=itarget;
        if actual==1
            targetVoltage=vtarget;
            [activeReference,voltagePI]=pi_step(vtarget-output,outerkp,outerki, ...
                p.Ts,voltagePI,0,maxI);
        end
        [command,currentPI]=pi_step(activeReference-feedback,kp/100,ki/100, ...
            p.Ts,currentPI,low,high);
    end
    % Compare-register truncation and complementary Boost IN mapping.
    if boost, duty=1-floor(p.arr*(1-command))/p.arr;
    else, duty=floor(p.arr*command)/p.arr;
    end
    if ~gate, duty=0; end
    if boost
        A=[-p.inductor_resistance/p.L,1/p.L,-(1-duty)/p.L; ...
           -1/p.C_low,-1/(p.source_resistance*p.C_low),0; ...
           (1-duty)/p.C_high,0,-1/(p.load_ohm*p.C_high)];
        b=[0;p.boost_input_V/(p.source_resistance*p.C_low);0];
    else
        A=[-p.inductor_resistance/p.L,-1/p.L,0; ...
           1/p.C_low,-1/(p.load_ohm*p.C_low),0;0,0,0];
        b=[duty*p.buck_input_V/p.L;0;0];
    end
    % Averaged continuous plant, RK4; digital controller runs once each 50us.
    a=A*x+b; b2=A*(x+p.Ts*a/2)+b;
    c=A*(x+p.Ts*b2/2)+b; d=A*(x+p.Ts*c)+b;
    x=x+p.Ts*(a+2*b2+2*c+d)/6;
    if boost || ~gate
        x(1)=max(0,x(1)); % Ideal passive freewheel; no reverse current while disabled.
    end
    peakCurrent=max(peakCurrent,abs(x(1)));
    if mod(n-1,stride)==0
        logged=logged+1; t(logged)=time;
        y(logged,:)=[output,feedback,x(1),duty,gate,actual,activeReference,targetVoltage];
    end
end
t=t(1:logged);y=y(1:logged,:);
meta=struct('plant','averaged CCM + ideal zero-current diode', ...
    'port_current_is_not_inductor_current',true,'boost',boost, ...
    'hardware_calibration_validated',false,'firmware_protection_full_model',false, ...
    'peak_inductor_A',peakCurrent);
end

function [output,state] = pi_step(error,kp,ki,Ts,state,low,high)
% Same conditional integration rule as ccs_project/pid.c, in duty fractions.
proportional=kp*error;
if ki==0
    state(1)=0;
elseif ~((proportional+state(1)>=high && error>0) || ...
         (proportional+state(1)<=low && error<0))
    state(1)=min(high,max(low,state(1)+ki*Ts*error));
end
output=min(high,max(low,proportional+state(1)));
state(2)=error;
end
