function plot_project_result(t,y,name,out)
% Plot the logged project response with separate duty and enable scales.
f = figure('Visible','off','Color','w','Position',[80 80 1150 850]);
tiledlayout(4,1);
nexttile; plot(t,y(:,1),t,y(:,8),'--'); grid on;
ylabel('Output / V'); legend('Output','Voltage target','Location','best');
title(strrep(name,'_',' '));
nexttile; plot(t,y(:,2),t,y(:,3),t,y(:,7),'--'); grid on;
ylabel('Current / A'); legend('Port feedback','Inductor','Current reference','Location','best');
nexttile;
yyaxis left; plot(t,100*y(:,4)); ylabel('Active duty / %');
yyaxis right; stairs(t,y(:,5)); ylim([-0.1 1.1]); yticks([0 1]); ylabel('Gate enable');
grid on; legend('Active-switch duty','Gate enable','Location','best');
nexttile;
modes=y(:,6); modes(modes==255)=4;
stairs(t,modes); ylim([-0.2 4.2]); yticks(0:4);
yticklabels({'Current','Dual','Open','Voltage','Waiting'});
grid on; ylabel('Control mode'); xlabel('Time / s');
exportgraphics(f,fullfile(out,[name '.png']),'Resolution',140);
close(f);
end
