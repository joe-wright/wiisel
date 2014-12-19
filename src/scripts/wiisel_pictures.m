RGB = imread('antonio_contrast.bmp', 'bmp');
fileID = fopen('antonnio_contrast.txt','w');
for i = 1:30
    for j = 1:30
        fprintf(fileID,'writeLEDUncomp(%d, %d, %d, %d, %d);\n',i-1, j-1, RGB(i, j, 1), RGB(i, j, 2), RGB(i, j, 3));
    end
end
%fclose(fileID);
%RGB = imread('lee.bmp', 'bmp');
%fileID = fopen('lee.txt','w');
%for i = 1:30
%    for j = 1:30
%        fprintf(fileID,'writeLEDUncomp(%d, %d, %d, %d, %d);\n',i-1, j-1, RGB(i, j, 1), RGB(i, j, 2), RGB(i, j, 3));
%    end
%end
%fclose(fileID);
RGB = imread('alberto.bmp', 'bmp');
fileID = fopen('alberto.txt','w');
for i = 1:30
    for j = 1:30
        fprintf(fileID,'writeLEDUncomp(%d, %d, %d, %d, %d);\n',i-1, j-1, RGB(i, j, 1), RGB(i, j, 2), RGB(i, j, 3));
    end
end
fclose(fileID);
RGB = imread('cal_centered.bmp', 'bmp');
fileID = fopen('cal_centered.txt','w');
for i = 1:30
    for j = 1:30
        fprintf(fileID,'writeLEDUncomp(%d, %d, %d, %d, %d);\n',i-1, j-1, RGB(i, j, 1), RGB(i, j, 2), RGB(i, j, 3));
    end
end
fclose(fileID);