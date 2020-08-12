#include "labelmaker.h"
#include <ui_labelmaker.h>
#include <ui_dirdialog.h>
using namespace std;

LabelMaker::LabelMaker(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LabelMaker),
    d_ui(new Ui::DirDialog),
    img_index(0),
    viewoffset(50),
    key(QDir::homePath()+"/.labelmaker.ini",QSettings::IniFormat)
{
    ui->setupUi(this);
    d_ui->setupUi(&dialog);
    dialog.setWindowFlags(Qt::WindowStaysOnTopHint);
    connectSignals();
    ui->graphicsView->setScene(&scene);
    readKey();
	dialog.show();
	installEventFilter(this);
}

void LabelMaker::readKey()
{
    int index = key.value("INDEX").toInt();
    QString imagedir = key.value("IMAGESDIR").toString();
    QString savedir = key.value("SAVEDIR").toString();
    if(index)
    {
        img_index = index;
    }
    if(imagedir.size() > 0)
    {
        d_ui->lineImageDir->setText(imagedir);
    }
    if(savedir.size() > 0)
    {
        d_ui->lineSaveTo->setText(savedir);
    }
}

void LabelMaker::saveKey()
{
    key.setValue("IMAGESDIR",d_ui->lineImageDir->text());
    key.setValue("SAVEDIR",d_ui->lineSaveTo->text());
    key.setValue("INDEX",img_index);
}

LabelMaker::~LabelMaker()
{
    writeText();
    saveKey();
    delete ui;
    delete d_ui;
}

void LabelMaker::onMouseMovedGraphicsView(int x, int y, Qt::MouseButton b)
{
    correctCoordiante(x,y);
    c_view.x = x;
    c_view.y = y;
    c_view.b = b;
    updateView();
}

void LabelMaker::onMousePressedGraphicsView(int mx, int my, Qt::MouseButton b)
{
	if(b == Qt::LeftButton)
	{
		if(ui->checkUseMI->checkState() != Qt::Checked)
		{
			correctCoordiante(mx,my);
			c_rect.x = mx;
			c_rect.y = my;
			c_rect.b = b;
			c_rect.flag = 1;
			updateView();
		}
	}
	if(b == Qt::RightButton)
	{
		if(bboxes.size() > 0)
		{
            auto itr = bboxes.begin();
            while(itr != bboxes.end())
            {
                Bbox *b = &(*itr);
                if( isPixelInBbox(mx,my,b) == true )
                {
                    itr = bboxes.erase(itr);
                    break;
                }
                else
                {
                    itr++;
                }
            }
			updateView();
		}
	}
}

void LabelMaker::onMouseReleasedGraphicsView(int mx, int my, Qt::MouseButton b)
{
    int r_x1,r_y1,r_x2,r_y2;
    if(b == Qt::LeftButton)
    {
        if(img_list.size() != 0)
        {
            if(ui->checkUseMI->checkState() == Qt::Checked)
            {
                //std::cout << "(x , y)" << mx << "," << my << std::endl;
                //std::cout << "(cols , rows )" << currentimg.cols << "," << currentimg.rows << std::endl;
                searchBboxByMI(mx,my, &r_x1, &r_y1, &r_x2, &r_y2);
                appendBbox(0, r_x1, r_y1, r_x2, r_y2);
            }
            else 
            {
                appendBbox(ui->spinLabelNumber->value(),c_rect.x,c_rect.y,c_view.x,c_view.y);
                if(ui->checkRange->checkState() == Qt::Checked)
                {
                    writeMultiText();
                    changeIndex(0);
                    ui->checkRange->setCheckState(Qt::Unchecked);
                }
            }
        }
        c_rect.flag = 0;
        updateView();

    }
    else if(b == Qt::RightButton)
    {
        updateView();
    }
}
void LabelMaker::convertAxsisGraphics2CurrentImage(int mx, int my, int *out_mx, int *out_my )
{
	int offset = viewoffset;
	mx -= offset;
	my -= offset;
    int w = scene_img_w;
    int h = scene_img_h;
    *out_mx = (int)((double)mx / w * currentimg.width());
    *out_my = (int)((double)my / h * currentimg.height());
}
void LabelMaker::searchBboxByMI(int mx, int my, int *out_x1, int *out_y1, int *out_x2, int *out_y2)
{
    //std::cout << "(x , y)" << mx << "," << my << std::endl;
	convertAxsisGraphics2CurrentImage(mx, my, &mx,&my);
    QImage mask = CreateMask();
	int max_x = 0;
	int max_y = 0;
    int max_r = 0;
    double max_mi = 0;
    QPoint maxp;
	QImage img = currentimg.convertToFormat(QImage::Format_RGB888);
	int r_min = 30 * ((double)my / img.height());
	int r_max = 90 * ((double)my / img.height());
	r_min = (r_min > 10)?r_min:10;
	r_max = (r_max > 20)?r_max:20;
    for(int r = r_min; r < r_max; r++)
    {
        double scale = r/100.;
        QImage ballmask = mask.scaled(mask.width()*scale, mask.height()*scale);
        int mw = ballmask.width();
        int mh = ballmask.height();
        //std::cout << "mw,mh = " << mw << "," << mh << std::endl;
		int size = 10;
		int step = 2;
        for(int x=mx-size; x < mx+size; x+=step)
		{
			if(((currentimg.width() - mw) <= x) || (x <= r)) continue;
			for(int y=my-size; y < my+size; y+=step)
			{
				if(((currentimg.height() - mh) <= y) || (y <= r) )continue;
				//qDebug() << "r:" << r << QString("(x,y) = (%1,%2)").arg(x).arg(y);
				double mi = calc_mi(img, ballmask, x-r, y-r);
				//std::cout << mi << std::endl;
				if(max_mi < mi)
				{
					max_x = mx;
					max_y = my;
					max_mi = mi;
					max_r = r;
					//qDebug() << QString("(x,y) = (%1,%2) : (mx,my) = (%3,%4) : r(%5)").arg(x).arg(y).arg(mx).arg(my).arg(r);
					//In max mi x1,y1,x2,y2
					//std::cout << "(x1 , y1)" << "(" << *out_x1 << "," << *out_y1 << ")" <<  "(x2 , y2)" << "(" << *out_x2 << "," << *out_y2 << ")" << std::endl;
				}
			}
		}
    }
    int offset = viewoffset;
    *out_x1 = (int)((double)(max_x - max_r)/currentimg.width() * scene_img_w)+offset;
    *out_y1 = (int)((double)(max_y - max_r)/currentimg.height() * scene_img_h)+offset;
    *out_x2 = (int)((double)(max_x + max_r)/currentimg.width() * scene_img_w)+offset;
    *out_y2 = (int)((double)(max_y + max_r)/currentimg.height() * scene_img_h)+offset;
}
double LabelMaker::calc_mi( const QImage &img, const QImage &maskimg, int x0, int y0 )
{
    int hist[256][2];
    for(int i=0; i<256; i++) {
        for(int j=0; j<2; j++) hist[i][j] = 0;
    }

    std::vector<int> pbg(256, 0); //BG
    std::vector<int> pfg(256, 0); //FG
    int cnt_fg = 0;
    const unsigned char *imdata = img.bits();
    int bpl = img.bytesPerLine();
    const unsigned char *maskdata = maskimg.bits();
    int maskbpl = maskimg.bytesPerLine();
    int mh = maskimg.height();
    int mw = maskimg.width();
	for(int i=0; i<mh; i++) {
        for(int j=0; j<mw; j++) {
			// qDebug() << QString("(mw,mh) = (%1,%2) ; (j,i) = (%3,%4)").arg(mw).arg(mh).arg(j).arg(i);
            // int gray = qGray(img.pixel(j+x0, i+y0));
            int gray = imdata[(j+x0) * 3 + (i+y0) * bpl];
            int mask = maskdata[j*3 + i*maskbpl];
            if (mask) {
                ++pfg[gray];
                ++cnt_fg;
                ++hist[gray][0];
            } else {
                ++pbg[gray];
                ++hist[gray][1];
            }
        }
    }
    int cnt = maskimg.width()*maskimg.height();
    double p_fg = (double)cnt_fg / (double)cnt;
    double jent = 0;
    double jx = 0;
    double jy = -p_fg * log(p_fg) - (1-p_fg) * log(1-p_fg);
    //std::cout << "pfg: " << pfg << std::endl;
    for(int i=0; i<pbg.size(); i++) {
        double px = 0;
        for(int j=0; j<2; j++) {
            if (hist[i][j] > 0) {
                double p = ((double)hist[i][j]/(double)cnt);
                jent += -p * log(p);
                px += p;
            }
        }
        if (px > 0) {
            jx += -px * log(px);
        }
    }
    //std::cout << "jx, jy, jent" << jx << " " << jy << " " << jent << std::endl;
    return jx + jy - jent;
}

QImage LabelMaker::CreateMask()
{
    QImage mask(250, 250, QImage::Format_RGB888);
    mask.fill(Qt::black);
    QPainter painter(&mask);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(125,125), 100,100);
    //mask.save("balltemp.png", "PNG");

    return mask;
}
void LabelMaker::resizeGraphicsView()
{
    updateView();
}

void LabelMaker::onPushNext()
{
    changeIndex(1);
}

void LabelMaker::onPushBack()
{
    changeIndex(-1);
}

void LabelMaker::onPushChooseDirectory()
{
    dialog.show();
}

void LabelMaker::onPushChooseImagesDir()
{
    img_index = 0;
    dialog.setWindowFlags(Qt::WindowStaysOnBottomHint);
    QDir dir = myq.selectDir(QDir(d_ui->lineImageDir->text()));
    d_ui->lineImageDir->setText(dir.path());
    d_ui->lineSaveTo->setText(dir.path()+"_labels");
    dialog.setWindowFlags(Qt::WindowStaysOnTopHint);
    dialog.show();
}

void LabelMaker::onPushChooseSaveTo()
{
    dialog.setWindowFlags(Qt::WindowStaysOnBottomHint);
    QDir dir = myq.selectDir(QDir(d_ui->lineImageDir->text()));
    d_ui->lineSaveTo->setText(dir.path());
    dialog.setWindowFlags(Qt::WindowStaysOnTopHint);
    dialog.show();
}

void LabelMaker::destroyDirDialog()
{
    img_list.clear();
    img_list = makeImageList(d_ui->lineImageDir->text());
    if(img_list.size() != 0)
    {
        loadImage();
        readText();
        updateView();
        ui->spinRangeMin->setMaximum(img_list.size()-1);
        ui->spinRangeMax->setMaximum(img_list.size());
    }
}

void LabelMaker::onPushPlus()
{
    ui->spinLabelNumber->setValue(ui->spinLabelNumber->value()+1);
}

void LabelMaker::onPushMinus()
{
    ui->spinLabelNumber->setValue(ui->spinLabelNumber->value()-1);
}

void LabelMaker::textChangedLinePage()
{
	bool ok;
	int page = ui->linePage->text().toInt(&ok, 10) -1;
	if(ok)
	{
		if(0 <= page && page < img_list.size())
		{
			changeIndex(page - img_index);
		}
	}
}

void LabelMaker::onSpinRangeValueChanged()
{
    if(ui->spinRangeMin->value() >= ui->spinRangeMax->value())
    {
        ui->spinRangeMax->setValue(ui->spinRangeMin->value() + 1);
    }
}

void LabelMaker::connectSignals()
{
    bool ret;
    ret = connect(ui->graphicsView,SIGNAL(mouseMoved(int,int,Qt::MouseButton)),this,SLOT(onMouseMovedGraphicsView(int,int,Qt::MouseButton)));
    assert(ret);
    ret = connect(ui->graphicsView,SIGNAL(mousePressed(int,int,Qt::MouseButton)),this,SLOT(onMousePressedGraphicsView(int,int,Qt::MouseButton)));
    assert(ret);
    ret = connect(ui->graphicsView,SIGNAL(mouseReleased(int,int,Qt::MouseButton)),this,SLOT(onMouseReleasedGraphicsView(int,int,Qt::MouseButton)));
    assert(ret);
    ret = connect(ui->graphicsView,SIGNAL(resized()),this, SLOT(resizeGraphicsView()));
    assert(ret);
    ret = connect(ui->pushNext,SIGNAL(clicked()),this,SLOT(onPushNext()));
    assert(ret);
    ret = connect(ui->pushBack,SIGNAL(clicked()),this,SLOT(onPushBack()));
    assert(ret);
    ret = connect(ui->pushChooseDirectory,SIGNAL(clicked()),this,SLOT(onPushChooseDirectory()));
    assert(ret);
    ret = connect(d_ui->pushChooseImageDir,SIGNAL(clicked()),this,SLOT(onPushChooseImagesDir()));
    assert(ret);
    ret = connect(d_ui->pushChooseSaveto,SIGNAL(clicked()),this,SLOT(onPushChooseSaveTo()));
    assert(ret);
    ret = connect(d_ui->pushOK,SIGNAL(clicked()),&dialog,SLOT(close()));
    assert(ret);
    ret = connect(&dialog,SIGNAL(finished(int)),this,SLOT(destroyDirDialog()));
    assert(ret);
    ret = connect(ui->pushPlus,SIGNAL(clicked()),this,SLOT(onPushPlus()));
    assert(ret);
    ret = connect(ui->pushMinus,SIGNAL(clicked()),this,SLOT(onPushMinus()));
    assert(ret);
	ret = connect(ui->linePage,SIGNAL(editingFinished()),this,SLOT(textChangedLinePage()));
    assert(ret);
    ret = connect(ui->spinRangeMin,SIGNAL(valueChanged(int)),this,SLOT(onSpinRangeValueChanged()));
    assert(ret);

}

int LabelMaker::updateView()
{
    scene.clear();
	scene.setSceneRect(0,0,ui->graphicsView->width(), ui->graphicsView->height());
    if(img_list.size() == 0)
    {
        return -1;
    }
    if(currentimg.isNull())
    {
        qDebug() << "img empty.";
        return -1;
    }
    setImage(currentimg);
    drawCursur();
    drawBbox();
    if(c_rect.flag == 1)
    {
        drawRect();
    }
    ui->labelFilename->setText(img_list[img_index].fileName());
	ui->linePage->setText(QString("%1").arg(img_index+1));
    ui->linePageNum->setText(QString("%1").arg(img_list.size()));
    ui->labelDebug->clear();
	ui->labelDebug->setText(
            QString("NUM_BBOX[%1]").arg(bboxes.size())
            + QString(" x:%1 y:%2").arg(c_view.x).arg(c_view.y) 
            + QString(" (size %1 x %2 )").arg(scene_img_w).arg(scene_img_h)
            );
    return 0;
}

int LabelMaker::setImage(const QImage &img)
{
    int offset = viewoffset;
    scene_img_w = ui->graphicsView->width()-offset*2;
    scene_img_h = ui->graphicsView->height()-offset*2;
    if(scene_img_w<=0 || scene_img_h<=0)
    {
        return -1;
    }
    QPixmap pix = QPixmap::fromImage(img.scaled(scene_img_w, scene_img_h));
    QGraphicsPixmapItem *p = scene.addPixmap(pix);
    p->setPos(offset,offset);
    return 0;
}

int LabelMaker::drawCursur()
{
	int r = 4;
	int offset = viewoffset;
    int w = scene_img_w;
    int h = scene_img_h;
	QPen p = QPen(myq.retColor(ui->spinLabelNumber->value())),QBrush(myq.retColor(ui->spinLabelNumber->value()));
    scene.addEllipse(c_view.x-(r/2),c_view.y-(r/2),r,r,p);
	if(ui->checkCrossLine->checkState() == Qt::Checked)
	{
		scene.addLine(c_view.x   ,offset+1          ,c_view.x   ,h-1+offset   ,p);
		scene.addLine(offset+1   ,c_view.y   ,w-1+offset ,c_view.y   ,p);
	}
    return 0;
}

int LabelMaker::drawRect()
{
    scene.addRect(QRect(QPoint(c_rect.x,c_rect.y),QPoint(c_view.x,c_view.y)),QPen(myq.retColor(ui->spinLabelNumber->value()),3));
    return 0;
}

int LabelMaker::drawBbox()
{
	int offset=viewoffset;
    int w = scene_img_w;
    int h = scene_img_h;
    for(int i=0;i<bboxes.size();i++)
    {
        int x1 = w * (bboxes[i].x - (bboxes[i].w/2));
        int y1 = h * (bboxes[i].y - (bboxes[i].h/2));
        int x2 = w * (bboxes[i].x + (bboxes[i].w/2));
        int y2 = h * (bboxes[i].y + (bboxes[i].h/2));
		x1 += offset;
		y1 += offset;
		x2 += offset;
		y2 += offset;
        scene.addRect(QRect(QPoint(x1,y1),QPoint(x2,y2)),QPen(QBrush(myq.retColor(bboxes[i].label)),3));
    }
    return 0;
}

void LabelMaker::loadImage()
{
    currentimg = QImage(img_list[img_index].filePath());
}

void LabelMaker::correctCoordiante(int &x, int &y)
{
	int offset = viewoffset;
    int w = scene_img_w;
    int h = scene_img_h;
    x = (x > offset) ?x:offset;
    y = (y > offset) ?y:offset;
    x = (x < w+offset-1) ?x:w+offset-1;
    y = (y < h+offset-1) ?y:h+offset-1;
}

QFileInfoList LabelMaker::makeImageList(QString path)
{
    QFileInfoList list,l_png,l_jpg;
    l_png = myq.scanFiles(path,"*png");
    l_jpg = myq.scanFiles(path,"*jpg");
    for(int i=0;i<l_png.size();i++)
    {
        list.append(l_png[i]);
    }
    for(int i=0;i<l_jpg.size();i++)
    {
        list.append(l_jpg[i]);
    }
    return list;
}

void LabelMaker::writeText()
{
    QString save_dir = d_ui->lineSaveTo->text();
    QDir dir(save_dir);
    QString fn = QString(dir.path()+"/"+img_list[img_index].fileName());
    fn.chop(4);
    fn = fn + ".txt";
    if (!dir.exists()){
        dir.mkpath(save_dir);
    }
    if(bboxes.size() > 0)
    {
        ofstream ofs;
        ofs.open(fn.toLocal8Bit());
        for(int i=0;i<bboxes.size();i++)
        {
            QString line = QString("%1 %2 %3 %4 %5").arg(bboxes[i].label).arg(bboxes[i].x).arg(bboxes[i].y).arg(bboxes[i].w).arg(bboxes[i].h);
            ofs << line.toStdString() << std::endl;
        }
        ofs.close();
    }
    else
    {
        QDir qd;
        qd.remove(fn);
    }
    bboxes.clear();
}

void LabelMaker::writeMultiText()
{
    if(bboxes.size() > 0)
    {
        for(int i=ui->spinRangeMin->value()-1;i<ui->spinRangeMax->value();i++)
        {
            if(i==img_index){
                continue;
            }
            QString save_dir = d_ui->lineSaveTo->text();
            QDir dir(save_dir);
            QString fn = QString(dir.path()+"/"+img_list[i].fileName());
            fn.chop(4);
            fn = fn + ".txt";
            if (!dir.exists()){
                dir.mkpath(save_dir);
            }
            ofstream ofs;
            ofs.open(fn.toLocal8Bit(), std::ios_base::app);
            QString line = QString("%1 %2 %3 %4 %5").arg(latestBox.label).arg(latestBox.x).arg(latestBox.y).arg(latestBox.w).arg(latestBox.h);
            ofs << line.toStdString() << std::endl;
            ofs.close();
        }
    }
}

void LabelMaker::readText()
{
    bboxes.clear();
    vector<string> lines;
    QString save_dir = d_ui->lineSaveTo->text();
    QDir dir(save_dir);
    QString fn = QString(dir.path()+"/"+img_list[img_index].fileName());
    fn.chop(4);
    fn = fn + ".txt";
    QFileInfo finfo(fn);
    if(finfo.exists())
    {
        ifstream ifs;
        ifs.open(fn.toLocal8Bit());
        string l;
        while(getline(ifs,l))lines.push_back(l);
        ifs.close();
    }
    for(int i=0;i<lines.size();i++)
    {
        QString qstr = QString::fromStdString(lines[i]);
        QStringList ql = qstr.split(" ");
        Bbox box;
        box.label = ql[0].toInt();
        box.x = ql[1].toFloat();
        box.y = ql[2].toFloat();
        box.w = ql[3].toFloat();
        box.h = ql[4].toFloat();
        bboxes.push_back(box);
    }
}

void LabelMaker::appendBbox(int label, int x1, int y1, int x2, int y2)
{
	int offset = viewoffset;
	x1 -= offset;
	y1 -= offset;
	x2 -= offset;
	y2 -= offset;
    int width = scene_img_w;
    int height = scene_img_h;
    int x = (x1+x2)/2;
    int y = (y1+y2)/2;
    int w = abs(x1 - x2);
    int h = abs(y1 - y2);
    latestBox.label = label;
    latestBox.x = (float)x/width;
    latestBox.y = (float)y/height;
    latestBox.w = (float)w/width;
    latestBox.h = (float)h/height;
    //qDebug() << QString("x:%1 ,y:%2 ,w:%3 ,h:%4").arg(latestBox.x).arg(latestBox.y).arg(latestBox.w).arg(latestBox.h);
    if((x&&y) && (w&&h))
    {
        bboxes.push_back(latestBox);
    }
}

void LabelMaker::changeIndex(int num)
{
    writeText();
    img_index+=num;
    if( img_index < 0)
    {
        img_index = 0;
    }
    if(img_list.size()-1 < img_index)
    {
        img_index = img_list.size()-1;
    }
    readText();
    loadImage();
    updateView();
}

bool LabelMaker::eventFilter(QObject *obj, QEvent *eve)
{
	QKeyEvent *key;
	if(eve->type() == QEvent::KeyRelease)
	{
		key = static_cast<QKeyEvent*>(eve);
		if(key->key()==0x1000014 || key->text()=="d")
		{
			onPushNext();
		}
		if(key->key()==0x1000012 || key->text()=="a")
		{
			onPushBack();
		}
		return true;
	}
	return false;
}

bool LabelMaker::isPixelInBbox(int x,int y,Bbox *box)
{
    int width = scene_img_w;
    int height = scene_img_h;
    int left   = (box->x - box->w/2) * width + viewoffset;
    int top    = (box->y - box->h/2) * height + viewoffset;
    int right  = (box->x + box->w/2) * width + viewoffset;
    int bottom = (box->y + box->h/2) * height + viewoffset;
    //qDebug() << "l:" << left << "t:"<<top << "r:"<<right << "b:"<<bottom;
    //qDebug() << "x:" << x << "y:" << y;
    bool ret = true;
    if(x<left || right<x)
    {
        ret = false;
    }
    if(y<top || bottom<y)
    {
        ret = false;
    }
    return ret;
}
