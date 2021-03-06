#include "action.h"

#include <QtWidgets>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <ctime>

ActionThread::ActionThread(QObject *parent) : QThread(parent) {
    abort = false;
    need_stop = false;
    steps = 1;
}

ActionThread::~ActionThread() {
    mutex_a.lock();
    abort = true;
    condition.wakeOne();
    mutex_a.unlock();
    wait();
}

std::pair<std::pair<unsigned int, unsigned int>, std::pair<double, double>> ActionThread::get_used_size() {
    mutex_a.lock();
    unsigned int  minw = *AntX, maxw = *AntX, minh = *AntY, maxh = *AntY;
    for (unsigned int i = 0; i != static_cast<unsigned int>(data->size()); ++i) {
        for (unsigned int j = 0; j != static_cast<unsigned int>(data->size()); ++j) {
            if ((*data)[i][j] != 0) {
                maxh = i;
                maxw = std::max(maxw, j);
                minw = std::min(minw, j);
            }
        }
    }
    mutex_a.unlock();
    return std::make_pair(std::make_pair(maxh - minh + 3, maxw - minw + 3),
                          std::make_pair(static_cast<double>(maxh + minh + 1) / 2,
                                         static_cast<double>(maxw + minw + 1) / 2));
}

void ActionThread::clear() {
    mutex_a.lock();
    for (size_t i = 0; i != data->size(); ++i) {
        std::fill((*data)[i].begin(), (*data)[i].end(), 0);
    }
    *AntWay = 0;
    *AntX = *AntY = static_cast<unsigned int>(data->size()) / 2;
    *need_steps = *did_steps = 0;
    mutex_a.unlock();
    emit did();
}

void ActionThread::new_rand(unsigned long long size) {
    mutex_a.lock();
    for (size_t i = 0; i != data->size(); ++i) {
        std::fill((*data)[i].begin(), (*data)[i].end(), 0);
    }
    *AntWay = 0;
    *AntX = *AntY = static_cast<unsigned int>(data->size()) / 2;
    srand(time(NULL));
    for (unsigned int x = *AntX - size; x != *AntX + size; ++x) {
        for (unsigned int y = *AntY - size; y != *AntY + size; ++y) {
            (*data)[x][y] = rand() % (*ColorsNum);
        }
    }
    *need_steps = *did_steps = 0;
    mutex_a.unlock();
    emit did();
}

void ActionThread::go() {
    //QMutexLocker locker(&mutex_a);
    need_stop = false;
    //if (need_stop) {
    //    qDebug("need stop");
    //} else {
    //    qDebug("not need stop");
    //}
    if (*need_steps == *did_steps) {
        return;
    }
    if (!isRunning()) {
        start(LowPriority);
    } else {
        condition.wakeOne();
    }
}

void ActionThread::stop(bool end) {
    need_stop = true;
    if (end) {
        mutex_a.lock();
        *need_steps = *did_steps;
        mutex_a.unlock();
    }
}

void ActionThread::set_data(std::vector<std::vector<char>>* data, std::vector<bool>* ways, unsigned int * ColorsNum,
                            unsigned int * AntX, unsigned int * AntY, unsigned int * AntWay,
                            long long * did_steps, long long * need_steps, bool * sync, Analyzer * analyzer) {
    mutex_a.lock();
    this->data = data;
    this->ways = ways;
    this->ColorsNum = ColorsNum;
    this->AntX = AntX;
    this->AntY = AntY;
    this->AntWay = AntWay;
    this->need_steps = need_steps;
    this->did_steps = did_steps;
    this->sync = sync;
    this->analyzer = analyzer;
    mutex_a.unlock();
}

void ActionThread::set_steps(unsigned int steps) {
    mutex_a.lock();
    if (steps > 10000) {
        this->steps = 10000;
    } else {
        this->steps = steps;
    }
    mutex_a.unlock();
}

void ActionThread::loader_noncompressed(QDataStream &stream) {
    quint32 AntX, AntY, AntWay, waysSize;
    qint64 did_steps, need_steps;
    stream >> waysSize;
    std::vector<bool> tmp_ways(waysSize);
    for (quint32 i = 0; i != waysSize; ++i) {
        bool tmp;
        stream >> tmp;
        tmp_ways[i] = tmp;
    }
    stream >> AntX >> AntY >> AntWay >> did_steps >> need_steps;
    quint64 dataSize;
    stream >> dataSize;
    quint32 minw, maxw, minh, maxh;
    stream >> minw >> maxw >> minh >> maxh;
    if (minw >= maxw || minh >= maxh) {
        throw std::invalid_argument("invalid data");
    }
    if (data->size() != dataSize) {
        data->clear();
        *data = std::vector<std::vector<char>> (static_cast<size_t>(dataSize),
                                               std::vector<char> (static_cast<size_t>(dataSize), 0));
    } else {
        for (size_t i = 0; i != data->size(); ++i) {
            std::fill((*data)[i].begin(), (*data)[i].end(), 0);
        }
    }
    for (quint32 i = minw; i != maxw; ++i) {
        for (quint32 j = minh; j != maxh; ++j) {
            quint8 tmp;
            stream >> tmp;
            (*data)[i][j] = tmp;
        }
    }
    *(this->ways) = tmp_ways;
    *(this->AntX) = AntX;
    *(this->AntY) = AntY;
    *(this->AntWay) = AntWay;
    *(this->did_steps) = did_steps;
    *(this->need_steps) = need_steps;
}

void ActionThread::loader_compressed(QDataStream &stream) {
    quint32 AntX, AntY, AntWay, waysSize;
    qint64 did_steps, need_steps;
    stream >> waysSize;
    std::vector<bool> tmp_ways(waysSize);
    for (quint32 i = 0; i != waysSize; ++i) {
        bool tmp;
        stream >> tmp;
        tmp_ways[i] = tmp;
    }
    stream >> AntX >> AntY >> AntWay >> did_steps >> need_steps;
    quint64 dataSize;
    stream >> dataSize;
    if (data->size() != dataSize) {
        data->clear();
        *data = std::vector<std::vector<char>> (static_cast<size_t>(dataSize),
                                               std::vector<char> (static_cast<size_t>(dataSize), 0));
    } else {
        for (size_t i = 0; i != data->size(); ++i) {
            std::fill((*data)[i].begin(), (*data)[i].end(), 0);
        }
    }
    quint8 color;
    quint32 i, j;
    quint64 size;
    stream >> size;
    for (quint64 c = 0; c != size; ++c) {
        stream >> i >> j >> color;
        (*data)[i][j] = color;
    }
    *(this->ways) = tmp_ways;
    *(this->AntX) = AntX;
    *(this->AntY) = AntY;
    *(this->AntWay) = AntWay;
    *(this->did_steps) = did_steps;
    *(this->need_steps) = need_steps;
}

bool ActionThread::load_data(QDataStream &stream) {
    try {
        mutex_a.lock();
        QString file_version;
        stream >> file_version;
        if (file_version == QString("LangtonAntDataFile v1.0")) {
            loader_noncompressed(stream);
            mutex_a.unlock();
            return true;
        } else if (file_version == QString("LangtonAntDataFile v1.0c")) {
            loader_compressed(stream);
            mutex_a.unlock();
            return true;
        }
        mutex_a.unlock();
        return false;
    } catch (...) {
        mutex_a.unlock();
        clear();
        return false;
    }
    return true;
}

void ActionThread::saver_noncompressed(QDataStream &stream,
                                       unsigned int minw, unsigned int maxw,
                                       unsigned int minh, unsigned int maxh) {
    stream << QString("LangtonAntDataFile v1.0");
    stream << (quint32)(static_cast<unsigned int>(ways->size()));
    for (auto elem : *ways) {
        stream << elem;
    }
    stream << (quint32)(*AntX);
    stream << (quint32)(*AntY);
    stream << (quint32)(*AntWay);
    stream << (qint64)(*did_steps);
    stream << (qint64)(*need_steps);
    stream << (quint64)(data->size());
    stream << (quint32)minw;
    stream << (quint32)maxw;
    stream << (quint32)minh;
    stream << (quint32)maxh;
    for (unsigned int i = minw; i != maxw; ++i) {
        for (unsigned int j = minh; j != maxh; ++j) {
            stream << (quint8)((*data)[i][j]);
        }
    }
}

void ActionThread::saver_compressed(QDataStream &stream, unsigned long long size) {
    stream << QString("LangtonAntDataFile v1.0c");
    stream << (quint32)(static_cast<unsigned int>(ways->size()));
    for (auto elem : *ways) {
        stream << elem;
    }
    stream << (quint32)(*AntX);
    stream << (quint32)(*AntY);
    stream << (quint32)(*AntWay);
    stream << (qint64)(*did_steps);
    stream << (qint64)(*need_steps);
    stream << (quint64)(data->size());
    stream << (quint64)(size);
    for (unsigned int i = 0; i != data->size(); ++i) {
        for (unsigned int j = 0; j != data->size(); ++j) {
            if ((*data)[i][j] != 0) {
                stream << (quint32)(i);
                stream << (quint32)(j);
                stream << (quint8)((*data)[i][j]);
            }
        }
    }
}

bool ActionThread::save_data(QDataStream &stream) {
    try {
        mutex_a.lock();
        bool first = true;
        unsigned int  minw = 0, maxw = 0, minh = 0, maxh = 0;
        unsigned long long non_zero = 0;
        for (unsigned int i = 0; i != data->size(); ++i) {
            for (unsigned int j = 0; j != data->size(); ++j) {
                if ((*data)[i][j] != 0) {
                    ++non_zero;
                    if (first) {
                        minw = maxw = i;
                        minh = maxh = j;
                        first = false;
                    } else {
                        maxw = i;
                        if (maxh < j) {
                            maxh = j;
                        }
                        if (minh > j) {
                            minh = j;
                        }
                    }
                }
            }
        }
        if ((unsigned long long)(maxh - minh) * (unsigned long long)(maxw - minw) <= non_zero * 9) {
            saver_noncompressed(stream, minw, ++maxw, minh, ++maxh);
        } else {
            saver_compressed(stream, non_zero);
        }
        mutex_a.unlock();
    } catch (...) {
        mutex_a.unlock();
        return false;
    }
    return true;
}

void ActionThread::run() {
    forever {
        //qDebug((std::to_string(*need_steps) + " " + std::to_string(*did_steps)).c_str());
        mutex_a.lock();
        for (unsigned int i = 0; i != steps; ++i) {
            if (need_stop) {
                break;
            }
            if (*AntX == 0 || *AntY == 0 || *AntX == static_cast<unsigned int>(data->size() - 1) || *AntY == static_cast<unsigned int>(data->size()) - 1) {
                if (*need_steps >= *did_steps) {
                    need_stop = true;
                    break;
                    *need_steps = *did_steps;
                }
            }
            if (*need_steps > *did_steps) {
                if (analyzer->isEnabled()) {
                    (*analyzer).add(*AntX, *AntY, *AntWay, (*data)[*AntX][*AntY]);
                }
                //qDebug("didaaa");
                ++(*did_steps);
                if ((*ways)[(*data)[*AntX][*AntY]]) {
                    *AntWay += 1;
                    *AntWay %= 4;
                } else {
                    if (*AntWay == 0) {
                        *AntWay = 3;
                    } else {
                        *AntWay -= 1;
                    }
                }
                ++(*data)[*AntX][*AntY];
                (*data)[*AntX][*AntY] %= *ColorsNum;
                if (*AntWay == 0) {
                    *AntY -= 1;
                } else if (*AntWay == 1) {
                    *AntX += 1;
                } else if (*AntWay == 2) {
                    *AntY += 1;
                } else if (*AntWay == 3) {
                    *AntX -= 1;
                }
            } else if (*need_steps < *did_steps) {
                //qDebug("dideeee");
                if (analyzer->isEnabled()) {
                    (*analyzer).pop();
                }
                --(*did_steps);
                if (*AntWay == 0) {
                    *AntY += 1;
                } else if (*AntWay == 1) {
                    *AntX -= 1;
                } else if (*AntWay == 2) {
                    *AntY -= 1;
                } else if (*AntWay == 3) {
                    *AntX += 1;
                }
                if ((*data)[*AntX][*AntY] == 0) {
                    (*data)[*AntX][*AntY] = static_cast<char>(*ColorsNum - 1);
                } else {
                    --(*data)[*AntX][*AntY];
                }
                if (!(*ways)[(*data)[*AntX][*AntY]]) {
                    *AntWay += 1;
                    *AntWay %= 4;
                } else {
                    if (*AntWay == 0) {
                        *AntWay = 3;
                    } else {
                        *AntWay -= 1;
                    }
                }
            } else {
                break;
            }
        }

        if (abort)
            return;

        if (analyzer->isAutoAnalyzerEnabled() && *need_steps == *did_steps) {
            emit show_and_restart();
        }

        if (need_stop) {
            need_stop = false;
            //qDebug("wait");
            condition.wait(&mutex_a);
            //qDebug("waited");
            mutex_a.unlock();
            continue;
        }

        if (abort)
            return;

        //qDebug("didd");
        if (*sync) {
            emit did();
            need_stop = true;
        } else {
            if (*need_steps == *did_steps) {
                need_stop = true;
            }
        }
        mutex_a.unlock();
    }
}

void ActionThread::change_point(long x, long y, char num, bool minus) {
    mutex_a.lock();
    if (x >= 0 && static_cast<size_t>(x) < data->size() && y >= 0 && static_cast<size_t>(y) < data->size()) {
        if (minus) {
            while ((*data)[x][y] < num) {
                (*data)[x][y] += *ColorsNum;
            }
            (*data)[x][y] = ((*data)[x][y] - num) % (*ColorsNum);
        } else {
            (*data)[x][y] = ((*data)[x][y] + num) % (*ColorsNum);
        }
    }
    mutex_a.unlock();
}
