function output = testRoboticSystem

serial_port = 'COM4';

s = instrfindall;
if isempty(s) || ~any(strcmp(s.Port,serial_port))
    s = serial(serial_port, 'DataBits', 8, 'Parity', 'none', 'StopBits', 1, 'Terminator', 'LF');
end

if length(s)>1 s = s(find(strcmp(s.Port,serial_port))); end

% Open serial port if not open
if ~strcmp(s.Status,'open')
    fopen(s);
end

% Clear read buffer
while s.BytesAvailable > 0
    resp = fgets(s); 
    disp(resp);
end

pause(2);

sendCommand(s,'0');
% pause(5);
% sendCommand(s,'A1-');
% sendCommand(s,'A2-');
timing = [];

output = [];
for j = 1:8
    for i = 1:12
        if (i == 12)
            j=j+1;
            for i = 1:12:-1:1
                nextWell = [char(64+j),num2str(i),'-'];
                t = sendCommand(s,nextWell);
                output = [output;j,i,t];
            end
        else
            nextWell = [char(64+j),num2str(i),'-'];
            t = sendCommand(s,nextWell);
            output = [output;j,i,t];    
        end
    end
end

%         pause(0.25);
fclose(s);
% 
% % Home
% fprintf(s,'0'); %home command  
% t = tic;
% while s.BytesAvailable == 0 pause(0.01); end
% ok = false;
% while s.BytesAvailable > 0
%     resp = fgets(s);
%     disp(resp);
%     if strfind(resp,'OK') 
%         ok = true;
%         disp(['Complete in ',num2str(toc(t)),' sec.']);
%     end
% end
% 

% disp(resp);

function ok = sendCommand(s, command)

% Clear buffer
% disp(s.BytesAvailable);
while s.BytesAvailable > 0
    resp = fgets(s);
    disp('Error');
end

disp(['Sending command: ',command]);
fprintf(s,command);  

t = tic;
while s.BytesAvailable == 0 pause(0.01); end
ok = false;
while s.BytesAvailable > 0
    resp = fgets(s);
    disp(resp);
    if strfind(resp,'OK') 
        ok = toc(t);
        disp(['Complete in ',num2str(toc(t)),' sec.']);
    end
    pause(0.1);
end
